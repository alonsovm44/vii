# Vii 🇲🇽 — The Mexican Programming Language

**A minimalist language that compiles to C.**

[![Version](https://img.shields.io/badge/version-1.3.0-blue.svg)](https://github.com/alonsovm44/vii)
[![Language](https://img.shields.io/badge/language-C-orange.svg)](https://en.wikipedia.org/wiki/C_(programming_language))
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)
[![Made in Mexico](https://img.shields.io/badge/🇲🇽_Made_in-Mexico-00A859.svg)]()
[![IDE Extension](https://img.shields.io/badge/VS_Code_Extension-Available-007ACC.svg)](https://github.com/alonsovm44/vii-lang-extension/releases)

## What Makes Vii Unique?

**Vii** (pronounced like *vee*) is a programming language born in Mexico that proves you don't need complex syntax to build powerful systems. It's designed for humans first—eliminating the punctuation soup that makes traditional languages intimidating.

### 🌟 Core Philosophy

1. **Zero Punctuation Fatigue**: No braces, brackets, semicolons, or commas. Python asks "why not?" — Vii asks **"why?"**
2. **Implicit Everything**: Values print automatically. Functions return the last expression. Code flows like thoughts.
3. **Bootstrapped**: Vii compiles itself. The compiler is written in Vii and generates optimized C.

### 🎯 Perfect For

- **DevOps** replacing messy bash scripts
- **Beginners** learning their first language
- **C developers** wanting faster iteration
- **Anyone** who believes code should read like poetry

## The 3 Rules

| Rule | What It Means | Example |
|------|---------------|---------|
| **Minimal Punctuation** | No `{ }`, `[ ]`, `;`, `,`, or `:` | `if x > 5` instead of `if (x > 5) {` |
| **Implicit Output** | Unsaved values print automatically | `"hello world"` → prints immediately |
| **Indentation Only** | Spaces define structure | 2 spaces = new block |

## Complete Vocabulary — Only 33 Keywords

You can learn Vii in an afternoon. The entire language fits in your head.

| Category | Keywords | Purpose |
|----------|----------|---------|
| **Control Flow** | `if`, `else`, `while`, `break` | Branching and loops |
| **Functions** | `do`, `->` | Define and return |
| **Data** | `list`, `dict`, `ref` | Collections and references |
| **Access** | `at`, `set`, `key` | Get, set, and key operations |
| **I/O** | `ask`, `put`, `append` | Read, write, append |
| **System** | `arg`, `paste`, `time`, `sys`, `env`, `exit` | CLI, modules, time, shell, vars |
| **Transform** | `len`, `type`, `slice`, `ord`, `chr`, `tonum`, `tostr` | Type conversion |
| **Logic** | `and`, `or` | Boolean operators |
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
./vii samples/demo.vii
./vii samples/fizzbuzz.vii
./vii samples/guess.vii arg1 arg2
```

### Compile to Native Binary
```bash
./vii samples/fizzbuzz.vii -o fizzbuzz    # Creates ./fizzbuzz
./fizzbuzz                                  # Runs native code
```

### CLI Options
```bash
vii --version           # Show version (1.3.0)
vii --help              # Show usage and all keywords  
vii --debug file.vii    # Generate debug_ast.json
vii -o binary file.vii  # Compile to native executable
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

```
Vii Source (.vii)
       ↓
   Lexer → Tokens
       ↓
   Parser → AST
       ↓
   CodeGen → C Code (.c)
       ↓
   GCC → Native Binary
```

**Self-Hosting**: The compiler itself is written in Vii (`src/bootstraping/main.vii`).

## Why Vii?

> *"We created Vii because we were tired of languages that need 10 symbols to print 'hello world'. Vii needs 2: quotes and the string itself."*

**Design Principles:**
- **Learnable in 1 hour** — Only 33 keywords
- **Writeable in 1 minute** — No punctuation to forget
- **Mexican** — Born in Mazatlan, built with Latin American pragmatism

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
