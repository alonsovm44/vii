# Vii Concept: Code Tables

## Syntax
Tables start with the `table` keyword followed by an identifier. The next lines must be indented and start with `|`.

```vii
table DB
  | ID | NAME    | do GREET s |
  | 1  | "Vii"   | "Hello " + s |
  | 2  | "Diego" | "Hi " + s |
```

## Translation
The Parser would translate the above into a `VAL_LIST` containing `VAL_DICT` objects.

1. **Headers**: The first row defines the keys for the dictionaries.
2. **Rows**: Every subsequent row is an entry.
3. **Functions**: If a header starts with `do`, the cell contains an anonymous function definition.

## Use Cases

### 1. Localization (Internationalization)
```vii
table lang
  | KEY      | EN      | ES      |
  | "greet"  | "Hello" | "Hola"  |
  | "bye"    | "Bye"   | "Adios" |

current = "ES"
"Greeting: " + lang at "greet" at current
```

### 2. Batch Processing
Useful for DevOps scripts where you need to run the same logic on different sets of metadata.

```vii
table servers
  | IP            | ENV    | do LOG s |
  | "192.168.1.1" | "PROD" | "CRITICAL: " + s |
  | "127.0.0.1"   | "DEV"  | "DEBUG: " + s |
```

## Implementation Notes
- **Lexer**: Needs to recognize `|` as `TOK_PIPE`.
- **Parser**: `ND_TABLE` node. It should consume everything until the `DEDENT`.
- **Interp**: Evaluate cells. If it's a `do` cell, create a `VAL_FUNC` (requires adding `VAL_FUNC` to the runtime).

## Why it fits Vii
- **Visual**: You see the relationships immediately.
- **Minimal**: Replaces dozens of `list set` and `dict key` calls.
- **Glue-code friendly**: Perfect for representing JSON-like data or CSVs directly in the source code.