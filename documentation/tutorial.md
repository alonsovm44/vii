# Getting Started with Vii

Welcome to **Vii**, a minimalist programming language designed to strip away the "noise" of modern software engineering. Vii isn't about how many characters you can type; it's about how many concepts you have to keep in your head. 

In Vii, there are exactly **26 words** to learn. No brackets, no semicolons, no boilerplate.

---

## 1. The Three Golden Rules
Before writing a single line of code, you must remember the core constraints of the language:

1.  **No Punctuation**: Forget `{ }`, `( )`, `[ ]`, `;`, `,`, or `:`. If you type a bracket, it's a syntax error.
2.  **Implicit Output & Return**: If a value isn't saved to a variable, it is printed to the screen automatically. Inside a function, the last thing evaluated is the return value.
3.  **Indentation Only**: Spaces are the only way to group code. Use 2 spaces per level.

---

## 2. Basic Data & Math

### Data Types
Vii handles two types of data:
- **Numbers**: Integers or Decimals (e.g., `5`, `-10`, `3.14`).
- **Text**: Strings wrapped in double quotes (e.g., `"hello"`).

### Variables & Assignment
Use `=` to save values.
```vii
name = "Diego"
age = 25
```

### No Operator Precedence!
**Crucial Rule**: Vii evaluates everything strictly **left-to-right**. 
In most languages, `2 + 3 * 4` is 14. In Vii, it is **20**.

```vii
# (2 + 3) = 5, then (5 * 4) = 20
result = 2 + 3 * 4
```
If you need to change the order, use intermediate variables:
```vii
temp = 3 * 4
result = 2 + temp
```

---

## 3. Control Flow

### Logic & Comparison
- `==` (Equals)

*Note: True is represented by `1` (though any non-zero value is considered "truthy"), and False is `0`.*

### If/Else
```vii
score = 85
if score > 90
  "Excellent"
else if score > 70
  "Good"
else
  "Try again"
```

### While Loops
```vii
count = 5
while count > 0
  count
  count = count - 1
```

---

## 4. Functions (`do`)
Define functions using the `do` keyword followed by the name and arguments.

```vii
do greet name greeting
  greeting + " " + name

greet "Alice" "Hello"
```
*Remember: There is no `return` keyword. The last line is the return value.*

---

## 5. Memory & Lists
Lists are the only data structure in Vii.

- `list`: Create a new list.
- `set`: Write to a list (`list set index value`).
- `at`: Read from a list (`list at index`).

```vii
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
```vii
"log.txt" put "Standard Log Entry"
```

---

## 7. Environment & Meta

### CLI Arguments
The `arg` keyword is a built-in list containing the command-line arguments.
```vii
first_arg = arg at 0
```

### Paste
Use `paste` to inject another file's source code into your current file at compile time.
```vii
paste "math_helpers.vii"
```

### Comments
Use `#` for single-line comments.

---

## 8. Conversion & Utilities
These keywords allow you to transform data types and inspect their properties.

- `len`: Returns the length of a string or a list.
- `ord`: Converts the first character of a string into its numeric ASCII/Unicode code.
- `chr`: Converts a numeric code into a single-character string.
- `tonum`: Parses a string into a number (useful for `ask` input).
- `tostr`: Converts a number into its string representation.

```vii
# Examples
len "Hello"      # 5
ord "A"          # 65
chr 66           # "B"
tonum "123"      # 123
tostr 42         # "42"
```

---

## 9. Optional Static Typing (Advanced)
For hiOptional Static Typing (Advanced)
For high-performance sections of your code, you can hint types to the transpiler. This allows Vii to generate native C variables instead of dynamic objects.

### Typed Variables
```vii
x num = 10
y num = 20
z num = x + y   # Compiles to native double z = x + y;
```

### Typed Functions
```vii
do add a->num b->num
  a + b
```

---

## 10. A Complete Example: FizzBuzz
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