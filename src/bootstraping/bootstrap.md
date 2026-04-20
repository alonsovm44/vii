# Vii Self-Hosting Analysis (Bootstrapping)

This document outlines the roadmap and technical requirements to move the Vii compiler from its current C implementation to a pure Vii implementation (`vii.vii`).

## 1. Current Progress (The v1.2 Foundation)
As of v1.2, we have the "Lexer Essentials":
- `ord` and `chr` for character/integer conversion.
- `slice` for substring extraction during tokenization.
- `len` for boundary checks.
- `dict` and `key` for the Symbol Table and AST node representation.
- `type` for handling polymorphic AST nodes.

## 2. Missing "Must-Have" Features
To fully implement the compiler without the overhead of C, we need the following from the v1.3/v1.4 candidate lists:

### A. Pass-by-Reference (`ref`)
**Reasoning**: Compilers pass around large structures (like the AST or the global scope table). Currently, Vii copies values when passed to functions. Passing a 1,000-node dictionary AST by value would crash performance and make in-place modification (like constant folding or optimization passes) impossible.

### B. String Utilities (`split`, `trim`, `replace`)
**Reasoning**: 
- `trim` is needed for the lexer to handle whitespace/newlines efficiently.
- `split` helps in processing source lines or path resolution.

### C. `safe` / `try` Error Handling
**Reasoning**: A compiler shouldn't just `exit(1)` when it hits a syntax error; it should ideally collect multiple errors or provide a clean stack trace. `safe` allows the compiler to attempt to parse a block and recover if it fails.

### D. Logic Grouping `( )`
**Reasoning**: While Vii aims for no punctuation, complex boolean logic in a parser (e.g., `if type == TOK_IDENT or type == TOK_NUM and precedence > current`) becomes unreadable without parentheses or massive amounts of intermediate variables.

## 3. The Bootstrapping Strategy

### Phase 1: The Lexer (`lexer.vii`)
- **Goal**: Read a string and output a list of `dict` objects (tokens).
- **Needs**: `slice`, `ord`, `len`.

### Phase 2: The Parser (`parser.vii`)
- **Goal**: Convert the token list into a recursive `dict` structure (The AST).
- **Needs**: `at`, `dict`, and references (`ref`) to link parent/child nodes without duplication.

### Phase 2.5: The Semantic Analyzer (`checker.vii`)
- **Goal**: Walk the AST to validate logic before code generation.
- **Tasks**: Check for return type mismatches and undefined variables.

### Phase 3: The Codegen (`codegen.vii`)
- **Goal**: Iterate the AST and emit C code.
- **Needs**: Heavy string concatenation or a `list` of strings that gets `join`ed at the end.

## 4. Feature Gap Checklist

| Feature | Status | Priority |
|---------|--------|----------|
| `dict`  | Done   | Critical |
| `slice` | Done   | Critical |
| `ref`   | Done         | High |
| `checker`| Todo  | High     |
| `split` | Todo   | Medium   |
| `for`   | Todo   | Medium   |
| `safe`  | Todo   | Low      |

## 5. Summary
To start the rewrite today, the most immediate blocker is **Pass-by-Reference**. Without it, the AST will be too memory-intensive. Once `ptr` is implemented, a minimal version of `vii.vii` can be written that emits the same C code the current `codegen.c` does.

### The "Bootstrap Loop"
1. `vii.exe` (C) compiles `vii.vii` -> `vii_v2.c`.
2. `gcc` compiles `vii_v2.c` -> `vii_v2.exe`.
3. `vii_v2.exe` (Vii) compiles `vii.vii` -> `vii_v3.c`.
4. If `vii_v2.c` and `vii_v3.c` are identical, Vii is officially self-hosted.