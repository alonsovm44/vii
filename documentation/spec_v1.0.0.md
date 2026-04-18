# IO Language Specification v1.0.0

IO is a concatenative, indentation-based, Turing-complete language designed for absolute concept minimalism. It aims to strip away "OS-level engineering artifacts" to expose pure, readable logic.

## 1. The Core Philosophy
IO measures minimalism by **concept count**, not character count. While many modern languages require learning dozens of concepts to start, IO requires exactly 8-10.

## 2. The 3 Inviolable Rules
1. **No Punctuation**: There are no `{ }`, `[ ]`, `( )`, `;`, `,`, or `:`. 
2. **Implicit Output & Return**: If a line results in a value and isn't assigned, it prints to the screen. The final evaluated line in a block or function is implicitly returned.
3. **Indentation Only**: Spaces are the only structural syntax used to group blocks of code.

## 3. The 21-Word Vocabulary
IO uses exactly 21 reserved words/symbols. These cannot be used as variable names.

### 3.1 Data
- **Numbers**: Integers or Decimals (e.g., `5`, `3.14`).
- **Text**: Strings wrapped in double quotes (e.g., `"hello"`).

### 3.2 Assignment & Logic
- `=`: Assigns the right side to the left side (suppresses implicit output).
- `==`: Equality check (Yields `-1` for true, `0` for false).

### 3.3 Math & Comparison
- `+`: Addition or string concatenation.
- `-`: Subtraction.
- `*`: Multiplication.
- `/`: Integer Division.
- `%`: Modulo (Remainder).
- `<`: Less than (Yields `-1` for true, `0` for false).
- `>`: Greater than (Yields `-1` for true, `0` for false).

### 3.4 Control Flow
- `if`: Begins a conditional block based on a logic evaluation.
- `else`: Fallback block for `if`.
- `while`: Repeats a block as long as the evaluation yields a non-zero value.

### 3.5 Abstraction
- `do`: Defines a function. 
  - **Syntax**: `do function_name arg1 arg2`

### 3.6 Memory
- `list`: Initializes an empty list.
- `at`: Reads from a list or string by index. (e.g., `mylist at 0`).
- `set`: Writes to a list by index. (e.g., `mylist set 0 100`).

### 3.7 Universal I/O
- `ask`: 
  - `input = ask`: Pauses for keyboard input.
  - `content = "file.txt" ask`: Reads the entire contents of a file.
- `put`: Writes data to a file (overwrites). 
  - **Syntax**: `"path.txt" put "data"`.

### 3.8 Environment & Meta
- `arg`: A built-in list containing CLI arguments.
- `paste`: Compiler directive to inject code from another file at compile-time.
- `#`: Single-line comment.

## 4. Technical Semantics

### 4.1 Truthiness
- `0` is **False**.
- Anything else (numbers, non-empty strings, non-empty lists) is **True**.

### 4.2 Operator Precedence
**IO has no operator precedence.** Expressions are evaluated strictly **left-to-right**.
Example: `2 + 3 * 4` evaluates to `20`, not `14`. 
To control evaluation order, use intermediate variables.

### 4.3 Function Calls
Functions are called by name followed by space-separated arguments.
Example: `result = my_function 10 20`

## 5. Formal Examples

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

### Fibonacci Function
```io
do fib n
  if n < 2
    n
  else
    fib n - 1 + fib n - 2

fib 10
```

### File Handling
```io
put "data.txt" "Line 1" + "\n" + "Line 2"
content = "data.txt" ask
content
```