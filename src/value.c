#include "vii.h"

Value *val_num(double n) {
    Value *v = arena_alloc(global_arena, sizeof(Value));
    v->kind = VAL_NUM; v->num = n;
    return v;
}

Value *val_str(const char *s) {
    Value *v = arena_alloc(global_arena, sizeof(Value));
    v->kind = VAL_STR; v->str = arena_strdup(global_arena, s);
    return v;
}

Value *val_bit(bool b) {
    Value *v = arena_alloc(global_arena, sizeof(Value));
    v->kind = VAL_BIT; v->num = b ? 1 : 0;
    return v;
}

Value *val_list(void) {
    Value *v = arena_alloc(global_arena, sizeof(Value));
    v->kind = VAL_LIST; v->item_count = 0; v->item_cap = 8;
    v->items = arena_alloc(global_arena, v->item_cap * sizeof(Value*));
    return v;
}

Value *val_ptr(Value *target) {
    Value *v = arena_alloc(global_arena, sizeof(Value));
    v->kind = VAL_PTR; v->target = target;
    return v;
}

Value *val_ref(Value *target) {
    Value *v = arena_alloc(global_arena, sizeof(Value));
    v->kind = VAL_REF; v->target = target;
    return v;
}

Value *val_none(void) {
    /* Every 'nada' instance is unique in the arena to prevent pointer 
       aliasing corruption if a user attempts to mutate the value in-place. */
    Value *v = arena_alloc(global_arena, sizeof(Value));
    v->kind = VAL_NONE;
    return v;
}

void val_list_grow(Value *v) {
    int old_cap = v->item_cap;
    v->item_cap = v->item_cap ? v->item_cap * 2 : 8;
    Value **new_items = arena_alloc(global_arena, v->item_cap * sizeof(Value*));
    if (v->items) memcpy(new_items, v->items, old_cap * sizeof(Value*));
    v->items = new_items;
}

bool val_truthy(Value *v) {
    if (!v) return false;
    switch (v->kind) {
        case VAL_NUM:  return v->num != 0;
        case VAL_STR:  return v->str[0] != '\0';
        case VAL_LIST: return v->item_count > 0;
        case VAL_BIT:  return v->num != 0;
        case VAL_PTR:  return v->target != NULL;
        case VAL_REF:  return val_truthy(v->target);
        case VAL_OUT:  return val_truthy(v->inner);
        case VAL_ENT:  return true;
        case VAL_UNI:  return true;
        default:       return false;
    }
}

Value* val_print_to(Value *v, FILE *f) {
    if (!v) return val_none();
    if (v->kind == VAL_REF) { val_print_to(v->target, f); return v; }
    if (v->kind == VAL_OUT) { val_print_to(v->inner, f); return v; }
    switch (v->kind) {
        case VAL_NUM:
            if (v->num == (long long)v->num)
                fprintf(f, "%lld", (long long)v->num);
            else
                fprintf(f, "%g", v->num);
            break;
        case VAL_STR:  fprintf(f, "%s", v->str); break;
        case VAL_LIST:
            fputc('[', f);
            for (int i = 0; i < v->item_count; i++) {
                if (i) fprintf(f, ", ");
                val_print_to(v->items[i], f);
            }
            fputc(']', f);
            break;
        case VAL_BIT:  fprintf(f, v->num ? "1" : "0"); break;
        case VAL_PTR:
            /* Print pointer address for debugging/introspection */
            fprintf(f, "0x%p", (void*)v->target);
            break;
        case VAL_ENT:
        case VAL_UNI:
            fprintf(f, "%s {", v->type_name);
            bool first = true;
            for (int i = 0; i < TABLE_SIZE; i++) {
                for (Entry *e = v->fields->buckets[i]; e; e = e->next) {
                    if (!first) fprintf(f, ", ");
                    fprintf(f, "%s: ", e->key);
                    val_print_to(e->val, f);
                    first = false;
                }
            }
            fprintf(f, "}");
            break;
        case VAL_NONE: fprintf(f, "nada"); break;
    }
    return v;
}

Value* val_print(Value *v) {
    val_print_to(v, stdout);
    return v;
}

const char* val_kind_name(int kind) {
    switch (kind) {
        case VAL_NUM:  return "num";
        case VAL_STR:  return "str";
        case VAL_LIST: return "list";
        case VAL_DICT: return "dict";
        case VAL_BIT:  return "bit";
        case VAL_REF:  return "ref";
        case VAL_OUT:  return "out";
        case VAL_PTR:  return "ptr";
        case VAL_NONE: return "nada";
        case VAL_ENT:  return "ent";
        case VAL_UNI:  return "uni";
        default:       return "unknown";
    }
}
