# Vii — A Minimalist Programming Language

 [Language extension for Windsurf / VS Code:](https://github.com/alonsovm44/vii-lang-extension/releases/tag/v0.0.2)

![Version](https://img.shields.io/badge/version-1.1.4-blue.svg)
![Language](https://img.shields.io/badge/language-C-orange.svg)
![License](https://img.shields.io/badge/license-MIT-green.svg)

# The one liner
>Vii is a Turing-complete compiled and interpreted programming language that replaces types and boilerplate with just 29 words.

Vii measures minimalism not by character count, but by concept count. No punctuation, no boilerplate — just logic.

## The 3 Rules

- **No Punctuation**: No `{ }`, `[ ]`, `( )`, `;`, `,`, or `:`. Punctuation is the #1 cause of beginner syntax errors.
- **Implicit Output & Return**: If a line results in a value and isn't saved, it prints automatically. Inside a function, the final evaluated line is implicitly returned.
- **Indentation Only**: Spaces are the only structural syntax used to group blocks of code.

## The Complete Vocabulary (29 Words)

1. **Data**: Numbers (`5`), Text (`"hello"`)
2. **Assignment & Logic**: `=`, `==`
3. **Math**: `+`, `-`, `*`, `/`, `%`
4. **Comparison**: `<`, `>`, `<=`, `>=`, `!=`
5. **Logic**: `and`, `or`
6. **Control Flow**: `if`, `else`, `while`
7. **Abstraction**: `do` (define function, implicit return)
8. **Memory**: `list`, `at`, `set`
9. **Universal I/O**: `ask` (keyboard OR file read), `put` (write to disk)
10. **Environment**: `arg` (CLI arguments list), `paste` (inject file at compile time)
11. **DevOps**: `sys` (run shell command), `env` (get environment variable), `exit` (terminate process)
12. **Conversion**: `len` (string/list length), `ord` (char → code), `chr` (code → char), `tonum` (string → number), `tostr` (number → string)
13. **Comments**: `#` (single line)

> The 18 keywords (`if`, `else`, `while`, `do`, `ask`, `list`, `at`, `set`, `put`, `arg`, `paste`, `len`, `ord`, `chr`, `tonum`, `tostr`, `and`, `or`) cannot be used as variable names.

## Build

```bash
gcc -Wall -O3 -std=c99 -o vii src/main.c src/ui.c src/value.c src/lexer.c src/parser.c src/interp.c src/codegen.c
```

Or with Make:
```bash
make
```

## Run

```bash
./vii examples/fizzbuzz.vii
./vii examples/guess.vii
./vii examples/lists.vii
./vii examples/fileio.vii
./vii examples/args.vii hello world
./vii examples/paste.vii
```

### CLI
```bash
vii --version    # prints 1.1.4
vii --help       # prints usage and vocabulary
vii --debug file.vii # generates debug_ast.json
```

## Compile

```bash
./vii examples/fizzbuzz.vii -o fizzbuzz 
```
This produces a C executable


## Examples

### FizzBuzz
```vii
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
```

### Number Guessing Game
```vii
do get_hint guess target
  if guess > target
    "Lower"
  else if guess < target
    "Higher"
  else
    "Correct!"

x = 0
"Think of a number between 1 and 10."
target = ask

while x == 0
  "What is your guess?"
  guess = ask
  get_hint guess target
  if guess == target
    x = 1
```

### Lists (with `set`)
```vii
nums = list
nums set 0 42
nums set 1 99
nums at 0
```

### File I/O
```vii
"test.txt" put "Hello from Vii!"
content = "test.txt" ask
content
```

### CLI Arguments
```vii
if arg at 0
  "First arg:"
  arg at 0
```

### Paste (Include)
```vii
paste "helper.vii"
result = double 21
```

### Conversion Built-ins
```vii
# len — length of string or list
len "hello"        # 5
nums = list
nums set 0 10
nums set 1 20
len nums           # 2

# ord — first character to numeric code
ord "A"            # 65

# chr — numeric code to single character
chr 65             # "A"

# tonum — parse string to number
tonum "42"         # 42
tonum "3.14"       # 3.14

# tostr — convert number to string
tostr 42           # "42"
```

## Standard Library

Vii ships with a modular standard library in `lib/`. Use `paste` to include any module:

```vii
paste "lib/std.vii"
paste "lib/math.vii"
```

### lib/std.vii — Core Utilities

| Function | Description |
|----------|-------------|
| `abs x` | Absolute value |
| `min a b` | Minimum of two numbers |
| `max a b` | Maximum of two numbers |
| `clamp val lo hi` | Clamp value to range |
| `sign x` | Sign: -1, 0, or 1 |
| `negate x` | Negate a number |
| `is_zero x` | Is value zero? |
| `is_positive x` | Is value positive? |
| `is_negative x` | Is value negative? |
| `swap a b` | Swap two values |
| `identity x` | Returns its input |

### lib/math.vii — Math Utilities

| Function | Description |
|----------|-------------|
| `pow base exp` | Exponentiation (integer exp) |
| `sqrt n` | Square root (Newton's method) |
| `even n` | Is even? |
| `odd n` | Is odd? |
| `factorial n` | Factorial |
| `fib n` | Fibonacci number |
| `gcd a b` | Greatest common divisor |
| `lcm a b` | Least common multiple |
| `percent part total` | Percentage |
| `avg a b` | Average of two numbers |
| `lerp a b t` | Linear interpolation |
| `floor n` | Floor of a number |

### lib/str.vii — String Utilities

| Function | Description |
|----------|-------------|
| `upper s` | Convert to uppercase |
| `lower s` | Convert to lowercase |
| `str_reverse s` | Reverse a string |
| `repeat s n` | Repeat string n times |
| `starts_with s prefix` | Check prefix |
| `ends_with s suffix` | Check suffix |
| `trim_left s` | Trim leading spaces |
| `trim_right s` | Trim trailing spaces |
| `trim s` | Trim both sides |
| `contains s sub` | Check if substring exists |
| `split s delim` | Split by single-char delimiter |
| `pad_left s len char` | Pad on the left |
| `pad_right s len char` | Pad on the right |
| `count_char s c` | Count character occurrences |

### lib/list.vii — List Utilities

| Function | Description |
|----------|-------------|
| `push lst val` | Append to list |
| `pop lst` | Remove last element |
| `first lst` | Get first element |
| `last lst` | Get last element |
| `contains lst val` | Check if value exists |
| `reverse lst` | Reverse list (new list) |
| `join lst sep` | Join elements with separator |
| `range start end` | Create range list |
| `fill val n` | Fill list with n copies |
| `sum lst` | Sum all elements |
| `slice lst start end` | Get slice of list |
| `count lst val` | Count occurrences |
| `index_of lst val` | Find first index (-1 if not found) |
| `remove_at lst idx` | Remove element at index |
| `map lst fn` | Map function over list |
| `filter lst fn` | Filter list by predicate |

### lib/io.vii — I/O Utilities

| Function | Description |
|----------|-------------|
| `append path data` | Append to file |
| `exists path` | Check if file exists |
| `println val` | Print with newline |
| `read_file path` | Read file as string |
| `write_file path data` | Write string to file |

### lib/random.vii — Random Utilities

| Function | Description |
|----------|-------------|
| `random_01 seed` | Random float 0.0–1.0 (LCG) |
| `random_int_in_range seed min max` | Random integer in range |

> **Note**: `random.vii` uses a Linear Congruential Generator. Pass a different seed each call for varied results.

### Idiomatic Vii Tips

- **No unary minus**: Use `0 - x` instead of `-x`
- **No operator precedence**: All operators evaluate left-to-right. Use intermediate variables for complex expressions:
  ```vii
  # Instead of: x == 5 and y == 10
  a = x == 5
  b = y == 10
  if a and b
    "both match"
  ```
- **`set` uses simple values**: Use intermediate variables for index/value:
  ```vii
  idx = len result
  result set idx current
  ```
- **Division is floating-point**: Use `%` (modulo) for integer remainder
