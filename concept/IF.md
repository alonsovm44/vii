# Compile-Time Conditionals & Constants for Vii

## The Problem

`sys` commands are OS-specific. There's no way to write cross-platform vii code:

```vii
# Windows only:
sys "dir /b src/*.c > _files.txt"
# Linux/Mac only:
sys "ls -1 src/*.c > _files.txt"
```

C solves this with `#ifdef _WIN32`. Vii needs its own answer — without punctuation.

---

## The Proposal: ALL_CAPS Convention

**Zero new keywords.** Instead, use casing to distinguish compile-time from runtime:

| Casing | Meaning | Mutable? | When resolved |
|--------|---------|----------|---------------|
| `name` | Variable | Yes | Runtime |
| `Name` | Variable | Yes | Runtime |
| `NAME` | Constant | **No** | Compile-time (built-in) or Runtime (user-defined) |

### Compile-Time Conditionals: `IF` / `ELSE IF` / `ELSE`

Uppercase `IF` is a **compile-time conditional**. It looks like `if` but is resolved during parsing. Dead branches are **never added to the AST**.

```vii
IF WIN
  cmd = "dir /b " + src + "\\*.c > " + tmp
ELSE IF UNIX
  cmd = "ls -1 " + src + "/*.c > " + tmp
ELSE
  cmd = "echo unsupported platform"
```

### Built-In Platform Constants

| Constant | True when |
|----------|-----------|
| `WIN` | Compiling on Windows (`_WIN32` defined) |
| `UNIX` | Compiling on Linux or macOS |

These are **not keywords** — they're identifiers that happen to be ALL_CAPS. The parser recognizes them only inside `IF` conditions. You can still use `win` or `unix` as variable names.

### User-Defined Constants

```vii
MAX_RETRIES = 3
PI = 3.14159
GREETING = "hello"
```

ALL_CAPS assignments are **immutable**. Attempting to reassign them is a compile-time error:

```vii
PI = 3.14159
PI = 3          # Error: cannot reassign constant PI
```

### CLI-Defined Flags

```bash
vii -DDEBUG file.vii
vii --define DEBUG file.vii
```

```vii
IF DEBUG
  "Running in debug mode"
  t_start = time
```

CLI flags make the identifier truthy inside `IF` conditions. They don't create runtime variables — they only exist at compile time.

---

## How It Works: Lexer & Parser

### The Key Insight

The lexer **already distinguishes** `if` from `IF`:

- `if` → matches keyword → `TOK_IF`
- `IF` → no keyword match → `TOK_IDENT` with text `"IF"`

**Zero lexer changes needed.** The parser just needs to recognize `TOK_IDENT("IF")` as a compile-time conditional.

### Parser Changes

In `parse_stmt`, before the existing `TOK_IF` handler, add:

```
if token is TOK_IDENT and text is "IF":
    read condition (next TOK_IDENT, must be ALL_CAPS)
    resolve condition at compile time
    if true:
        parse body block normally → inject into AST
    if false:
        skip body block (consume tokens, build no AST nodes)
    check for "ELSE" or "ELSE IF" chains
```

The `ELSE` and `ELSE IF` sequences are also just identifiers:
- `TOK_IDENT("ELSE")` followed by `TOK_IDENT("IF")` → compile-time else-if
- `TOK_IDENT("ELSE")` alone → compile-time else

### Constant Enforcement

In `parse_stmt` for assignments:
- If left-hand side is ALL_CAPS and the variable already exists in the scope → compile-time error: "cannot reassign constant NAME"
- ALL_CAPS assignments are stored in the same `Table` but flagged (add a `bool constant` field to `Entry`, or maintain a separate constant set)

### Compile-Time Resolution

For `IF` conditions, the parser checks:

1. **Built-in**: `WIN` → `#ifdef _WIN32`, `UNIX` → `#ifndef _WIN32`
2. **CLI-defined**: Check the `-D` flag list passed from `main.c`
3. **Unknown identifier**: Treat as falsy (no error — allows forward-compatible code)

---

## Side-by-Side: `if` vs `IF`

| | `if` | `IF` |
|---|------|------|
| **When resolved** | Runtime | Compile-time (during parsing) |
| **Condition** | Any expression | Only ALL_CAPS identifiers |
| **Dead branch** | Parsed into AST, not executed | **Completely discarded** |
| **Can use variables** | Yes | No — only constants/flags |
| **Indentation** | Yes | Yes (same style) |
| **Can nest** | Yes | Yes |
| **Keyword?** | Yes (`TOK_IF`) | No (`TOK_IDENT("IF")`) |

---

## Real-World Example: bundle.vii

```vii
# bundle.vii — Merge all .c files in a directory into one monolithic file
paste "lib/str.vii"

do strip_cr s
  result = ""
  i = 0
  while i < len s
    c = s at i
    if ord c != 13
      result = result + c
    i = i + 1
  result

SRC = "src"
OUT = "monolithic.c"
TMP = "_files.txt"

IF WIN
  cmd = "dir /b " + SRC + "\\*.c > " + TMP
ELSE IF UNIX
  cmd = "ls -1 " + SRC + "/*.c > " + TMP

sys cmd

raw = TMP ask
raw = strip_cr raw
nl = chr 10
files = split raw nl

merged = ""
i = 0
while i < len files
  fname = files at i
  fname = trim fname
  if len fname > 0
    path = SRC + "/" + fname
    content = path ask
    merged = merged + content + nl + nl
  i = i + 1

OUT put merged

IF WIN
  sys "del " + TMP
ELSE IF UNIX
  sys "rm " + TMP

"Created " + OUT + " from " + tostr len files + " source files"
```

---

## Why This Beats the Alternatives

### vs. `when` keyword
- `when` adds a new keyword. `IF`/`ELSE` reuses existing words with casing.
- `when` is unfamiliar. `IF`/`ELSE` is instantly understood.
- The ALL_CAPS convention is universal: C macros, Go exports, Ruby constants, Python PEP 8.

### vs. `#if` / `#when` directives
- `#` is already comments in vii — confusing overload.
- Directives create a sub-language with different rules.
- Breaks the "no punctuation" principle.

### vs. `platform` built-in (runtime check)
- Dead branch still gets parsed/compiled.
- Dead branch might reference commands/files that don't exist on current OS.
- No way to add compile-time flags.
- Every systems language uses compile-time conditionals for a reason.

### vs. `env "OS"` runtime check
- Same problems as `platform` — runtime, not compile-time.
- Environment variables are unreliable across platforms.

---

## Implementation Checklist

### Lexer (`lexer.c`)
- [x] **No changes needed** — `IF`/`ELSE` already lex as `TOK_IDENT`

### Parser (`parser.c`)
- [x] Add `parse_when_stmt()` — handles `IF`/`ELSE IF`/`ELSE` compile-time blocks
- [x] Add `is_all_caps()` helper — checks if identifier text is all uppercase
- [x] Add `resolve_condition()` — checks built-in platforms + CLI defines
- [x] Add `skip_block()` — consume tokens until matching DEDENT without building AST
- [x] In `parse_stmt()`, check for `TOK_IDENT("IF")` before falling through
- [x] In assignment handling, enforce immutability for ALL_CAPS names

### Main (`main.c`)
- [x] Add `-D` / `--define` CLI flag parsing
- [x] Pass defines list to parser (global or via Parser struct)

### Interpreter (`interp.c`)
- [x] In `ND_ASSIGN`, check if target is ALL_CAPS and already defined → runtime error

### Codegen (`codegen.c`)
- [x] **No changes** — `IF` blocks are resolved before AST exists
- [x] Constants: runtime protection added to emitted C header.

### Header (`vii.h`)
- [x] Add `char **cli_defines` and `int cli_define_count` globals
- [x] Add `bool is_constant` to `Entry` struct

---

## Keyword Count Impact

**+0 new keywords.** `IF`, `ELSE`, `ELSE IF` are already words in the language — just different casing. The ALL_CAPS convention is a **naming rule**, not a new keyword.

The only "addition" is the `-D` CLI flag, which is an interface change, not a language change.

---

## Open Questions

1. **Should user-defined constants (`PI = 3.14`) be usable in `IF` conditions?**
   - Leaning toward **no** — only built-in platforms and `-D` flags work in `IF`. Constants are just immutable runtime values. This keeps `IF` simple and predictable.

2. **Should `IF` conditions support `and`/`or`?**
   - e.g., `IF DEBUG and WIN` — this would require a mini expression parser for compile-time.
   - Leaning toward **no for v1.2** — use nested `IF` instead. Keep it minimal.

3. **Should `IF` be allowed inside functions?**
   - Yes — it's resolved at parse time, so it works anywhere. Entire functions could be platform-specific:
   ```vii
   do clear_screen
     IF WIN
       sys "cls"
     ELSE IF UNIX
       sys "clear"
   ```

4. **What about `IF` inside `if`?**
   - Yes — `IF` is independent of `if`. They can nest freely:
   ```vii
   if verbose
     IF WIN
       "Windows verbose mode"
     ELSE IF UNIX
       "Unix verbose mode"
   ```

5. **Error on unknown `IF` condition?**
   - Leaning toward **no** — treat unknown identifiers as falsy. This lets you write:
   ```vii
   IF HAS_GRAPHICS
     # code that only exists if a graphics lib is available
   ```
   Without defining `HAS_GRAPHICS`, the block is simply skipped.
