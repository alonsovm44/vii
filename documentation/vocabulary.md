# Vii Vocabulary Reference (v1.2.5)

Vii measures minimalism by concept count. There are exactly 48 words/symbols/concepts to learn.

## 1. Control Flow

### Runtime
| Word | Use | Description |
| :--- | :--- | :--- |
| `if` | `if x == 1` | Executes block if condition is non-zero. |
| `else` | `else` | Fallback for `if` or `while` (via `else if`). |
| `while` | `while x < 10` | Repeats block while condition is non-zero. |
| `break` | `break` | Immediately exits the current `while` loop. |

### Compile-Time (Macros)
| Word | Use | Description |
| :--- | :--- | :--- |
| `IF` | `IF WIN` | Resolved during parsing. Dead branches are discarded. |
| `ELSE IF`| `ELSE IF UNIX`| Conditional fallback for compile-time `IF`. |
| `ELSE` | `ELSE` | Fallback for compile-time `IF`. |

## 2. Abstraction
| Word | Use | Description |
| :--- | :--- | :--- |
| `do` | `do func x y` | Defines a function. Supports return type hints: `do func->num`. |
| `->` | `x->num`, `x->ref` | Type hint operator. Used in function params and return types. |

## 3. Memory & Data Structures
| Word | Use | Description |
| :--- | :--- | :--- |
| `list` | `l = list` | Creates a new dynamic array. |
| `dict` | `d = dict` | Creates a new key-value map. |
| `ref` | `y = ref x` | Creates a reference (alias) to a variable instead of a copy. |
| `at` | `l at 0` | Retrieves value at index (list) or key (dict). |
| `set` | `l set 0 v` | Updates value at numeric index in a list. |
| `key` | `d key "k" v` | Updates value at string key in a dictionary. |

## 4. Universal I/O
| Word | Use | Description |
| :--- | :--- | :--- |
| `ask` | `ask` / `path ask` | Reads from stdin (keyboard) or reads a file if a path is provided. |
| `put` | `path put data` | Writes data to a file. Overwrites by default. |
| `append`| `path put d append`| Flag for `put` to add data to the end of a file instead of overwriting. |

## 5. Environment & DevOps
| Word | Use | Description |
| :--- | :--- | :--- |
| `arg` | `arg at 0` | Built-in list of Command Line Arguments. |
| `paste` | `paste "lib.vii"` | Injects source code from another file at compile time. |
| `time` | `time` | Returns the current Unix epoch timestamp (number). |
| `sys` | `sys "ls"` | Executes a shell command and returns the exit code. |
| `env` | `env "PATH"` | Retrieves an environment variable string. |
| `exit` | `exit 0` | Terminates the process with the given exit code. |

## 6. Metadata & Comments
| Word | Use | Description |
| :--- | :--- | :--- |
| `#` | `# comment` | Single-line comment. |
| `#{ }#`| `#{ block }#` | Multiline comment block. |

## 6. Conversion & Introspection
| Word | Use | Description |
| :--- | :--- | :--- |
| `len` | `len x` | Returns length of string, list, or dictionary. |
| `type` | `type x` | Returns string representation of type ("num", "str", "list", "dict"). |
| `slice`| `slice s 0 5` | Extracts a sub-portion of a string or list. |
| `ord` | `ord "A"` | Returns numeric ASCII/Unicode code of the first character. |
| `chr` | `chr 65` | Returns a single-character string from a numeric code. |
| `tonum`| `tonum "5"` | Converts a string to a number. |
| `tostr`| `tostr 5` | Converts a number to a string. |

## 7. Logic & Math Operators

### Assignment
| Symbol | Description |
| :--- | :--- |
| `=` | Assigns value to a variable. ALL_CAPS names become immutable constants. |

### Comparison (Works on numbers and strings)
| Symbol | Description |
| :--- | :--- |
| `==` | Equal to |
| `!=` | Not equal to |
| `<` | Less than |
| `>` | Greater than |
| `Lte` | Less than or equal to |
| `Gte` | Greater than or equal to |

### Arithmetic (Numbers only)
| Symbol | Description |
| :--- | :--- |
| `+` | Addition (or String Concatenation if one side is a string). |
| `-` | Subtraction (or Unary Minus if at start of expression). |
| `*` | Multiplication. |
| `/` | Division. |
| `%` | Modulo (Remainder). |

### Grouping
| Symbol | Description |
| :--- | :--- |
| `(` | Begins a grouped expression to override left-to-right evaluation. |
| `)` | Ends a grouped expression. |

### Boolean Logic
| Word | Description |
| :--- | :--- |
| `and` | True if both sides are non-zero. Short-circuits. |
| `or` | True if either side is non-zero. Short-circuits. |

## 8. Special Platform Constants
Used specifically with the compile-time `IF` macro:
- `WIN`: Truthy if compiling on a Windows environment.
- `UNIX`: Truthy if compiling on Linux, macOS, or other Unix-like systems.

---
Examples:
```vii
# a simple dev diary CLI tool
cmd = arg at 0 

do init 
  "Making new devlog file...\n"
  file str = "devlog.md"
  file put "Devlog kept with diaryx. Written in Vii"

do log 
  message = arg at 1
  file str = "devlog.md"
  file put ("\n\n" + time + "\n") append 
  file put message append 

do main cmd->str
  if cmd == "init"
    init
  else if cmd == "log"
    log

main cmd

```
```vii


G = 0.0000000000067 

do fg m1 m2 r
  m1 * m2 * G / (r * r)

m1 = 12
m2 = 24
r = 100

fg m1 m2 r


```

```vii
# FizzBuzz in VII
IF WIN 
  x = 1
  while x < 101 
    if x % 15 == 0
      "FizzBuzz"
    else if x % 3 == 0
      "Fizz"
    else if x % 5 == 0
      "Buzz"
    else
      x
    x = x + 1
ELSE IF UNIX
 
  do fizzbuzz 
  ls = list
  x = 1
  while x < 101 
    if x % 15 == 0
      ls append "FizzBuzz"
    else if x % 3 == 0
      ls append "Fizz"
    else if x % 5 == 0
      ls append "Buzz"
    else
      ls append x
    x = x + 1
  ls
   
  fizzbuzz
ENDIF

```