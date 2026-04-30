1. The "Static Buffer Clobber" Bug (Segfault waiting to happen)
File: codegen.c (Shape Tracker)

c

static char* cify_name(const char *name) {
    static char buf[256]; // <--- DANGER
    // ...
    return strdup(buf);
}
The Bug: Because buf is static, it is shared across the entire program. In gen_struct_name, you do this:

c

char *c_func = cify_name(func_name); // Writes to buf, allocates copy
char *c_var = cify_name(var_name);   // OVERWRITES buf before strdup is safe? 
                                     // Actually, strdup happens inside, so it's safe HERE...
Wait, it's actually safe inside gen_struct_name because strdup copies it before the second call. BUT, if two threads call this, or if you ever call cify_name twice in one line elsewhere, it will silently corrupt strings.
The Fix: Just remove the word static. Put it on the stack:

c

char buf[256]; // Stack allocated, completely thread-safe and clobber-proof
2. The Shape Tracker Does Nothing (Dead Code)
File: codegen.c -> emit_c_expr
You wrote a magnificent ~200-line Shape Tracker that analyzes dictionaries and generates FastShape_... C structs. But you never actually use them.
If a user writes:

vii

d = dict
d key "name" "Vii"
x = d at "name"
Your tracker successfully figures out that d is rigid, generates a beautiful C struct in the header... but then emit_c_expr for ND_AT just does this:

c

case ND_AT:
    fprintf(f, "runtime_at("); // <--- Still doing a slow Hash Table lookup!
The Fix: In emit_c_expr under ND_AT, you need to check can_use_fast_shape(), and if true, cast the Value pointer and access the field directly:

c

case ND_AT: {
    if (n->left->kind == ND_VAR && n->right->kind == ND_STR) {
        const char *var = n->left->name;
        const char *key = n->right->str;
        // Find the current function name (requires passing context to emit_c_expr)
        if (can_use_fast_shape(current_func, var, key)) {
            char *field = cify_name(key);
            fprintf(f, "((FastShape_%s_%s*)%s)->%s", cify_name(current_func), cify_name(var), var, field);
            free(field);
            break;
        }
    }
    fprintf(f, "runtime_at("); // ... fallback to normal
3. Memory Leak Bomb in Transpiled Code
File: codegen.c -> emit_c_header
Your interpreter is brilliant because it uses an Arena—when the script ends, the OS reclaims all memory in one big chunk. Zero leaks.
But your transpiled C code uses raw malloc and strdup everywhere (val_str, runtime_split, runtime_str_concat, runtime_slice).
If a user compiles a Vii script to a binary and runs a while 1 loop that manipulates strings, the binary will leak hundreds of megabytes per minute and eventually get OOM-Killed by the OS.
The Fix: You need to either:

Write a simple 50-line Mark-and-Sweep Garbage Collector for the transpiled C.
Or, transpile an Arena allocator into the generated C code, just like you did for the interpreter.
4. [FIXED] The val_none() Singleton Trap
File: value.h

c

The Bug: Returning a static pointer caused corruption when mutating values through pointers (e.g., if a pointer targeting 'nada' was used for assignment). 

The Fix: Updated val_none() in value.c to use arena_alloc, ensuring every 'nada' has a distinct address.

5. String Concatenation Buffer Overflow
File: codegen.c -> Transpiled runtime_binop

c

if(op==TOK_PLUS && (l->kind==VAL_STR||r->kind==VAL_STR)){ 
    char b[2048]; 
    sprintf(b, "%s%s", l->kind==VAL_STR?l->str:"", r->kind==VAL_STR?r->str:""); 
    return val_str(b); 
}
The Bug: If a user reads a 10MB log file using ask "log.txt" and then concatenates it with another string big_file + " end", sprintf will write past the 2048 byte b buffer. This is a classic Stack Buffer Overflow. It will crash the transpiled binary, or worse, open a security vulnerability.
(Note: Your interpreter version in interp.c uses 512 byte buffers for concatenation, so it has the same bug, just a smaller bomb).
The Fix: Calculate the length first, malloc the exact size, sprintf into it, then free it. (You actually already did this correctly in runtime_str_concat! Just replace the sprintf line in runtime_binop to call runtime_str_concat instead).

6. 
Your interpreter handles this flawlessly. But if you compile that exact script to a binary using -o, it will crash or silently fail.

Look at your transpiler in codegen.c under emit_c_functions:

c

for (int i = 0; i < fn->def->body_count && i < argc; i++) {
    table_set(scope, fn->def->body[i]->name, argv[i]);
}
It completely ignores the ptr type tag! It just passes the value normally. When the transpiled C code tries to do target = target + 1, it won't mutate the original val. It will just overwrite the local target variable. The implicit "pointer" magic is lost in translation.

The Fix:
You need to add the same ptr check from your interpreter into the transpiled C function header. Right inside emit_c_functions, change that argument binding loop to this:

c

for (int i = 0; i < fn->def->body_count && i < argc; i++) {
    // Check if parameter expects a pointer
    if (fn->def->body[i]->type_tag && strcmp(fn->def->body[i]->type_tag, "ptr") == 0) {
        fprintf(f, "  table_set(env, \"%s\", val_ref(table_get(parent, \"%s\"))); \n", 
                fn->def->body[i]->name, n->body[i]->name);
    } else {
        fprintf(f, "  if(argc > %d) table_set(env, \"%s\", argv[%d]);\n", i, fn->def->body[i]->name, i);
    }
}
(Note: Passing parent to table_get works perfectly here because runtime_var_get in your transpiled C unwraps references automatically when doing math on them!)