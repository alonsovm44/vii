# Getting Started with IO

Welcome to **IO**, a minimalist programming language designed to strip away the "noise" of modern software engineering. IO isn't about how many characters you can type; it's about how many concepts you have to keep in your head. 

In IO, there are exactly **21 words** to learn. No brackets, no semicolons, no boilerplate.

---

## 1. The Three Golden Rules
Before writing a single line of code, you must remember the core constraints of the language:

1.  **No Punctuation**: Forget `{ }`, `( )`, `[ ]`, `;`, `,`, or `:`. If you type a bracket, it's a syntax error.
2.  **Implicit Output & Return**: If a value isn't saved to a variable, it is printed to the screen automatically. Inside a function, the last thing evaluated is the return value.
3.  **Indentation Only**: Spaces are the only way to group code. Use 2 spaces per level.

---

## 2. Basic Data & Math

### Data Types
IO handles two types of data:
- **Numbers**: Integers or Decimals (e.g., `5`, `-10`, `3.14`).
- **Text**: Strings wrapped in double quotes (e.g., `"hello"`).

### Variables & Assignment
Use `=` to save values.
```io
name = "Diego"
age = 25
```

### No Operator Precedence!
**Crucial Rule**: IO evaluates everything strictly **left-to-right**. 
In most languages, `2 + 3 * 4` is 14. In IO, it is **20**.

```io
# (2 + 3) = 5, then (5 * 4) = 20
result = 2 + 3 * 4
```
If you need to change the order, use intermediate variables:
```io
temp = 3 * 4
result = 2 + temp
```

---

## 3. Control Flow

### Logic & Comparison
- `==` (Equals)
- `<` (Less than)
- `>` (Greater than)

*Note: True is represented by `-1` (or any non-zero value), and False is `0`.*

### If/Else
```io
score = 85
if score > 90
  "Excellent"
else if score > 70
  "Good"
else
  "Try again"
```

### While Loops
```io
count = 5
while count > 0
  count
  count = count - 1
```

---

## 4. Functions (`do`)
Define functions using the `do` keyword followed by the name and arguments.

```io
do greet name greeting
  greeting + " " + name

greet "Alice" "Hello"
```
*Remember: There is no `return` keyword. The last line is the return value.*

---

## 5. Memory & Lists
Lists are the only data structure in IO.

- `list`: Create a new list.
- `set`: Write to a list (`list set index value`).
- `at`: Read from a list (`list at index`).

```io
colors = list
colors set 0 "Red"
colors set 1 "Blue"

colors at 0   # Prints "Red"
```

---

## 6. Universal I/O

### The `ask` Polymorphism
The `ask` keyword is smart. It pulls data into your program based on context:
1. **Keyboard**: `name = ask` (waits for user input).
2. **File**: `content = "data.txt" ask` (reads the entire file).

### The `put` Keyword
To write to a file, use `put`.
```io
put "log.txt" "Standard Log Entry"
```

---

## 7. Environment & Meta

### CLI Arguments
The `arg` keyword is a built-in list containing the command-line arguments.
```io
first_arg = arg at 0
```

### Paste
Use `paste` to inject another file's source code into your current file at compile time.
```io
paste "math_helpers.io"
```

### Comments
Use `#` for single-line comments.

---

## 8. A Complete Example: FizzBuzz
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