# Vii — A Minimalist Programming Language

 [Language extension for Windsurf / VS Code:](https://github.com/alonsovm44/vii-lang-extension/releases/tag/v0.0.2)

![Version](https://img.shields.io/badge/version-1.0.0-blue.svg)
![Language](https://img.shields.io/badge/language-C-orange.svg)
![License](https://img.shields.io/badge/license-MIT-green.svg)

# The one liner
>Vii is a Turing-complete compiled and interpreted programming language that replaces types and boilerplate with just 20 words.

Vii measures minimalism not by character count, but by concept count. No punctuation, no boilerplate — just logic.

## The 3 Rules

- **No Punctuation**: No `{ }`, `[ ]`, `( )`, `;`, `,`, or `:`. Punctuation is the #1 cause of beginner syntax errors.
- **Implicit Output & Return**: If a line results in a value and isn't saved, it prints automatically. Inside a function, the final evaluated line is implicitly returned.
- **Indentation Only**: Spaces are the only structural syntax used to group blocks of code.

## The Complete Vocabulary (29 Words)

1. **Data**: Numbers (`5`), Text (`"hello"`)
2. **Assignment & Logic**: `=`, `==`
3. **Math**: `+`, `-`, `*`, `/`, `%`
4. **Comparison**: `<`, `>`
5. **Control Flow**: `if`, `else`, `while`
6. **Abstraction**: `do` (define function, implicit return)
7. **Memory**: `list`, `at`, `set`
8. **Universal I/O**: `ask` (keyboard OR file read), `put` (write to disk)
9. **Environment**: `arg` (CLI arguments list), `paste` (inject file at compile time)
10. **DevOps**: `sys` (run shell command), `env` (get environment variable), `exit` (terminate process)
11. **Conversion**: `len` (string/list length), `ord` (char → code), `chr` (code → char), `tonum` (string → number), `tostr` (number → string)
12. **Comments**: `#` (single line)

> The 16 keywords (`if`, `else`, `while`, `do`, `ask`, `list`, `at`, `set`, `put`, `arg`, `paste`, `len`, `ord`, `chr`, `tonum`, `tostr`) cannot be used as variable names.

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
vii --version    # prints 1.1.3
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
