# Vii 

**A minimalist language with gradual typing and stack allocation.**

[![Version](https://img.shields.io/badge/version-1.4.0-blue.svg)](https://github.com/alonsovm44/vii)
[![Language](https://img.shields.io/badge/language-C-orange.svg)](https://en.wikipedia.org/wiki/C_(programming_language))
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)
[![Made in Mexico](https://img.shields.io/badge/🇲🇽_Made_in-Mexico-00A859.svg)]()
[![IDE Extension](https://img.shields.io/badge/VS_Code_Extension-Available-007ACC.svg)](https://github.com/alonsovm44/vii-lang-extension/releases)

## What Makes Vii Unique?

**Vii** is an in-development lightweight low level scripting language. Whether you are new into coding or a seasoned dev, vii works for any public.

### 🌟 Core Philosophy

1. **No Punctuation Fatigue**: No braces, brackets, semicolons, or commas. Python asks "why not?" — Vii asks **"why?"**
2. **Implicitness**: Values print automatically. Functions return the last expression. Code flows like thoughts.

### Good For

- **DevOps** replacing messy bash scripts
- **Beginners** learning their first language
- **C developers** wanting faster iteration
- **Anyone** who wants the UX of a compiled language but the ease of a scripting one.

## The 4 Rules

| Rule | What It Means | Example |
|------|---------------|---------|
| **Minimal Punctuation** | No `{ }`, `[ ]`, `,`, or `:` | `if x > 5` instead of `if (x > 5) {` |
| **Implicit Output** | Unsaved values print automatically | `"hello world"` prints immediately |
| **Indentation Only** | Spaces define structure | 2 spaces = new block |
| **Gradual Typing** | Types when you need them | `x i32 = 5` or just `x = 5` |

## Complete Vocabulary — 40+ Keywords

You can learn Vii in an afternoon. The entire language fits in your head.

| Category | Keywords | Purpose |
|----------|----------|---------|
| **Control Flow** | `if`, `else`, `while`, `break`, `for`, `in` | Branching and loops |
| **Functions** | `do`, `->` | Define and return |
| **Data** | `list`, `dict`, `ref`, `ptr`, `bit` | Collections and references |
| **Memory** | `stack_alloc` | Stack allocation for fixed arrays |
| **Types** | `i8`, `i16`, `i32`, `i64`, `u8`, `u16`, `u32`, `u64`, `f32`, `f64` | Primitive numeric types |
| **Access** | `at`, `set`, `key`, `keys` | Get, set, and key operations |
| **I/O** | `ask`, `askfile`, `put`, `append` | Read, write, append |
| **System** | `arg`, `paste`, `time`, `sys`, `env`, `exit` | CLI, modules, time, shell, vars |
| **Transform** | `len`, `type`, `slice`, `ord`, `chr`, `tonum`, `tostr`, `split`, `trim`, `replace` | Type conversion |
| **Logic** | `and`, `or`, `not` | Boolean operators |
| **Safety** | `safe` | Safe navigation operator |
| **Macros** | `IF`, `ELSE`, `WIN`, `UNIX` | Compile-time conditionals |
| **Math** | `+`, `-`, `*`, `/`, `%`, `=`, `==`, `!=`, `<`, `>`, `Lte`, `Gte` | Operators |
| **Comments** | `#`, `#{ }#` | Line and block comments |

> 💡 **Convention**: `UPPERCASE` = immutable constants/macros, `lowercase` = variables/functions

## Quick Start

### Build from Source
```bash
# Single command
gcc -O3 src/*.c -o vii -lm

# Or use Make
make
```

### Run a Script
```bash
./vii samples/simple/demo.vii
./vii samples/simple/fizzbuzz.vii
./vii samples/simple/guess.vii arg1 arg2
./vii samples/advanced/stack_alloc.vii   # Fixed arrays demo
```

### CLI Options
```bash
vii --version           # Show version (1.4.0)
vii --help              # Show usage and all keywords  
vii --debug file.vii    # Generate debug_ast.json
vii file.vii            # Run interpreter (default)
```

## Example: FizzBuzz in Vii

```vii
# FizzBuzz — the classic interview question

do fizzbuzz n
  while n Lte 100
    if n % 15 == 0
      "fizzbuzz"
    else if n % 3 == 0
      "fizz"
    else if n % 5 == 0
      "buzz"
    else
      n
    n = n + 1

fizzbuzz 1
```

Notice: **No braces. No semicolons. No commas.** Just logic.

## Standard Library

Include modules with `paste`:

```vii
paste "lib/std.vii"   # Core utilities
paste "lib/math.vii"  # Math functions
paste "lib/str.vii"   # String helpers
paste "lib/list.vii"  # List operations
paste "lib/io.vii"    # File I/O
paste "lib/random.vii" # Random numbers
```

| Module | Function | Description |
|--------|----------|-------------|
| **std** | `abs x`, `min a b`, `max a b` | Basic utilities |
| **std** | `clamp val lo hi`, `sign x` | Range operations |
| **math** | `pow base exp`, `sqrt n` | Exponents & roots |
| **math** | `factorial n`, `fib n`, `gcd a b` | Number theory |
| **str** | `upper s`, `lower s`, `trim s` | String transforms |
| **str** | `split s delim`, `contains s sub` | String search |
| **list** | `push lst val`, `pop lst` | List operations |
| **list** | `map lst fn`, `filter lst fn` | Functional tools |
| **io** | `read_file path`, `write_file path data` | File I/O |
| **random** | `random_01`, `random_int_in_range min max` | Random numbers |

## Architecture

Vii 1.4 is now an interpreter with a gradual type system:

```
Vii Source (.vii)
       ↓
   Lexer → Tokens
       ↓
   Parser → AST (with type checking)
       ↓
   Interpreter → Execute
```

### Type System Features

**Primitive Types:** `i8`, `i16`, `i32`, `i64`, `u8`, `u16`, `u32`, `u64`, `f32`, `f64`

**Gradual Typing:**
```vii
# Untyped - inferred at runtime
x = 42

# Explicitly typed
y i32 = 42
z f64 = 3.14159
```

**Stack Allocation for Fixed Arrays:**
```vii
# Fixed-size array on stack (bounded)
buf u8[1024] = stack_alloc    # 1KB buffer
nums i32[100] = stack_alloc   # 100 integers

# Access with bounds checking
buf at 0 = 65        # OK
buf at 1024 = 66     # Runtime error: out of bounds
```

**Explicit Casting:**
```vii
x = 3.14159
y = x -> i32    # Cast to i32 (value truncated)
```
## Bootstrapping

Progress has been made in writing Vii in Vii (look at `src/bootstrapping/` folder). Contributing would help a lot thanks!

## IDE Support

- **[VS Code / Windsurf Extension](https://github.com/alonsovm44/vii-lang-extension/releases)**
- Syntax highlighting
- Auto-indentation
- Keyword snippets

---

<p align="center">
  Made with 💚 in Mexico 🇲🇽<br>
  <a href="https://github.com/alonsovm44/vii">GitHub</a> •
  <a href="https://github.com/alonsovm44/vii-lang-extension">VS Code Extension</a> •
  <a href="LICENSE">MIT License</a>
</p>
