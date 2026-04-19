# Vii: The Core Philosophy: Concept Minimalism
Vii measures minimalism not by character count, but by concept count. Python requires learning ~30 core concepts to build a basic program. Vii requires exactly 8.

# Lang spec 1.1

## The 3 Rules of Vii

- No Punctuation: No { }, [ ], ( ), ;, ,, or :. Punctuation is the #1 cause of beginner syntax errors.
- Implicit Output & Return: If a line results in a value and isn't saved, it prints to the screen automatically. Inside a function, the final evaluated line is implicitly returned. No print() or return functions.
- Indentation Only: Spaces are the only structural syntax used to group blocks of code.

## The Complete Vocabulary (21 Words)
Vii has no keywords for class, return, import, true, false, try, catch, or variable types (0 is false, anything else is true).

1. Data: Numbers (5), Text ("hello")
2. Assignment & Logic: `=`, `==`
3. Math: `+`, `-`, `*`, `/`, `%`
4. Comparison: `<`, `>`
5. Control Flow: `if`, `else`, `while`
6. Abstraction: `do` (define function, implicit return)
7. Memory: `list`, `at`, `set`
8. Universal I/O: `ask` (input from keyboard OR read file), `put` (write to disk)
9. Environment: `arg` (CLI arguments list), `paste` (inject file at compile time)
10. Comments: `#` (single line)
>(Note: The 21 keywords/operators cannot be used as variable names to keep the compiler infinitely simple. To read a file, pass its path to ask: data = "file.txt" ask. To append, read it, concatenate, and put it back)


## Turing Completeness Proof

To be Turing complete, a language only needs conditional branching (if), dynamic memory allocation (list, set), and infinite repetition (while). Here is FizzBuzz in vii to demonstrate density vs. readability:
```vii
# this is a comment
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
## Why This Works
Beginners don't struggle with logic; they struggle with typing. They don't understand why they need public static void main(String[] args) just to say "Hello".

Vii strips the OS-level engineering artifacts away from the syntax. You start typing logic on line 1, character 1. It proves that a language can be rigorously defined, completely capable of universal computation, and still feel like writing a to-do list.