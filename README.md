# IO — A Minimalist Programming Language

 [Language extension for Windsurf / VS Code:](https://github.com/alonsovm44/io-lang-extension/releases/tag/v0.0.2)

![Version](https://img.shields.io/badge/version-1.0.0-blue.svg)
![Language](https://img.shields.io/badge/language-C-orange.svg)
![License](https://img.shields.io/badge/license-MIT-green.svg)

# The one liner
>IO is a Turing-complete compiled and interpreted programming language that replaces types and boilerplate with just 20 words.

IO measures minimalism not by character count, but by concept count. No punctuation, no boilerplate — just logic.

## The 3 Rules

- **No Punctuation**: No `{ }`, `[ ]`, `( )`, `;`, `,`, or `:`. Punctuation is the #1 cause of beginner syntax errors.
- **Implicit Output & Return**: If a line results in a value and isn't saved, it prints automatically. Inside a function, the final evaluated line is implicitly returned.
- **Indentation Only**: Spaces are the only structural syntax used to group blocks of code.

## The Complete Vocabulary (26 Words)

1. **Data**: Numbers (`5`), Text (`"hello"`)
2. **Assignment & Logic**: `=`, `==`
3. **Math**: `+`, `-`, `*`, `/`, `%`
4. **Comparison**: `<`, `>`
5. **Control Flow**: `if`, `else`, `while`
6. **Abstraction**: `do` (define function, implicit return)
7. **Memory**: `list`, `at`, `set`
8. **Universal I/O**: `ask` (keyboard OR file read), `put` (write to disk)
9. **Environment**: `arg` (CLI arguments list), `paste` (inject file at compile time)
10. **Conversion**: `len` (string/list length), `ord` (char → code), `chr` (code → char), `tonum` (string → number), `tostr` (number → string)
11. **Comments**: `#` (single line)

> The 16 keywords (`if`, `else`, `while`, `do`, `ask`, `list`, `at`, `set`, `put`, `arg`, `paste`, `len`, `ord`, `chr`, `tonum`, `tostr`) cannot be used as variable names.

## Build

```bash
gcc -Wall -O3 -std=c99 -o io src/io.c
```

Or with Make:
```bash
make
```

## Run

```bash
./io examples/fizzbuzz.io
./io examples/guess.io
./io examples/lists.io
./io examples/fileio.io
./io examples/args.io hello world
./io examples/paste.io
```

### CLI
```bash
io --version    # prints 1.0.0
io --help       # prints usage and vocabulary
```

## Examples

### FizzBuzz
```io
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
```io
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
```io
nums = list
nums set 0 42
nums set 1 99
nums at 0
```

### File I/O
```io
put "test.txt" "Hello from IO!"
content = "test.txt" ask
content
```

### CLI Arguments
```io
if arg at 0
  "First arg:"
  arg at 0
```

### Paste (Include)
```io
paste "helper.io"
result = double 21
```

### Conversion Built-ins
```io
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
