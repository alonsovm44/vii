# IO — A Minimalist Programming Language

IO measures minimalism not by character count, but by concept count. No punctuation, no boilerplate — just logic.

## The 3 Rules

- **No Punctuation**: No `{ }`, `[ ]`, `( )`, `;`, `,`, or `:`. Punctuation is the #1 cause of beginner syntax errors.
- **Implicit Output & Return**: If a line results in a value and isn't saved, it prints automatically. Inside a function, the final evaluated line is implicitly returned.
- **Indentation Only**: Spaces are the only structural syntax used to group blocks of code.

## The Complete Vocabulary (17 Words)

- **Data**: Numbers (`5`), Text (`"hello"`)
- **Assignment**: `=`
- **Math/Logic**: `+ - * / % < > ==`
- **Control Flow**: `if`, `else`, `while`
- **Abstraction**: `do` (define function)
- **Memory**: `ask` (keyboard input), `list`, `at`
- **Comments**: `#` (single line)

> The 8 keywords (`if`, `else`, `while`, `do`, `ask`, `list`, `at`, `set`) cannot be used as variable names.

## Build

```bash
gcc -Wall -O2 -std=c99 -o io src/io.c
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

### Lists
```io
nums = list
nums at 0 = 42
nums at 1 = 99
nums at 0
```
