# Vii C-Parity Roadmap

**Goal**: Achieving C semantics with Python syntax. This roadmap tracks features required to make Vii a viable replacement for C in systems programming while maintaining its "scripting" ergonomics.

---

## 1. Type System Maturity (High Priority)
To map C exactly, Vii needs to handle types that don't fit into the "everything is a Value struct" model.

- [x] **Unions (`uni`)**: Overlapping memory regions.
  - Syntax: `uni Name;  a i32;  b f32`
  ```vii
  uni myunion
    thing f32
    yyyyy i32 
    string str 

  ```

- [ ] **Enums**: Named integer constants.
  - Convention: `enum Color \n  RED \n  BLUE` (automatically assigned 0, 1, etc.)
- [ ] **Type Aliases (`typedef`)**: Allowing `typeset MyInt = i32`.
- [ ] **Void Type**: Support for `void` and generic pointers `ptr void` for raw memory handling.
- [x] **Null type**: Nada = Null
## 2. Memory Layout & `ent` Evolution (High Priority)
Currently, `ent` fields are stored in a `Table`. For C parity, they must support contiguous binary layouts.

- [ ] **Contiguous Ents**: A `packed` attribute to ensure an `ent` occupies exactly `sizeof(fields)` bytes in memory.
  - Required for: Binary file parsing, network protocols.
- [ ] **`offsetof` Operator**: `offsetof Entity field` to get byte offsets.
- [ ] **Alignment Control**: `ent Name aligned 16` for SIMD-friendly structures.
- [ ] **Static Variables**: Variables that persist across function calls, scoped to the function or file.

## 3. Bitwise & Low-Level Operations (Medium Priority)
Systems programming requires direct bit manipulation which is currently missing.

- [ ] **Bitwise Operators**: 
  - `&` (AND), `|` (OR), `^` (XOR), `~` (NOT).
  - `<<` (Left Shift), `>>` (Right Shift).
- [ ] **Pointer Arithmetic**:
  - Explicit `ptr-add` and `ptr-sub` methods.
  - Support for `ptr_a - ptr_b` to get element distance.
- [ ] **Fixed-size Arrays in Ents**: Supporting `field u8[32]` inside an `ent` definition rather than a dynamic list.

## 4. Control Flow Parity (Medium Priority)
C's efficiency often comes from specific jump and branch logic.

- [ ] **`switch` / `match`**: A more efficient alternative to `if/else` chains for integer/enum branching.
- [ ] **`defer`**: Vii's version of C cleanup patterns (or `finally`). Essential for manual memory safety.
  - Example: `p = heap_alloc 10; defer heap_free p`.

## 5. FFI & Interoperability (High Priority)
Vii must be able to talk to existing C libraries without a wrapper.

- [ ] **`extern` Declarations**:
  - `extern "printf" i32 (ptr u8, ...)`
- [ ] **Calling Conventions**: Support for `stdcall` vs `cdecl` on Windows.
- [ ] **Library Loading**: `paste` currently handles source, but we need `link "libname"` or `dlopen` ergonomics for shared objects.

## 6. Preprocessor Power (Low Priority)
Vii's `IF` macros handle some of this, but C's preprocessor is more flexible.

- [ ] **Constant Expressions**: Ensuring `X = 10 + 5` is resolved at compile time if both are constants.
- [ ] **Stringification**: A way to turn an identifier name into a string at compile time.

---

## Technical Debt & Refactoring

### The "Boxed Value" Problem
Currently, every `Value` in the interpreter is a large struct:
```c
typedef struct Value {
    ValKind kind;
    double num;
    char  *str;
    // ... many more fields ...
} Value;
```
To reach true C performance, the **Compiler (Phase 11)** must be able to "unbox" primitive types (`i32`, `f32`) into raw CPU registers rather than carrying the `Value` overhead everywhere.

### Strict vs Soft Types
- **Soft**: `x = 5` (Inferred as `num` or `i32`, allows reassignment to `str`).
- **Hard**: `x i32 = 5` (Locked to `i32` for its entire lifetime).
C parity requires **Hard** types for any performance-critical or memory-mapped code.

---

## Completion Criteria
1. Can Vii parse its own `vii.h` file and generate equivalent `ent` structures?
2. Can Vii call `sqlite3_open` from the standard SQLite shared library without C glue code?
3. Can a Vii program traverse a linked list of 1 million nodes using `heap_alloc` without exhausting memory due to "Value" boxing?

*Last Updated: April 29, 2026*