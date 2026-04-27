# Vii 1.4 Study Syllabus

> Master these concepts to fully understand and extend Vii 1.4

---

## Phase 1: Foundations (Prerequisites)

### 1.1 C Memory Layout
**Study:** How programs use memory
- Stack segment (automatic, limited, fast)
- Heap segment (manual, large, slower)
- Data/BSS segments (globals, statics)
- Code segment (read-only)

**Practice:**
```c
// Write a C program that allocates on both stack and heap
// Print addresses to see where they land
// Use `size` and `nm` commands to inspect binaries
```

**Key Insight:** Stack grows down, heap grows up. They meet in the middle (simplification, but useful mental model).

---

### 1.2 Pointers in C
**Study:** The core concept Vii's `ptr` mimics
- `&` address-of operator
- `*` dereference operator
- Pointer arithmetic (`ptr + 1` moves by sizeof(type))
- `void*` vs typed pointers
- NULL pointers and segmentation faults

**Practice:**
```c
int x = 10;
int *p = &x;
int **pp = &p;  // Double pointer
printf("x=%d, *p=%d, **pp=%d\n", x, *p, **pp);
```

**Key Insight:** A pointer is just a variable holding a memory address. The type tells the compiler how many bytes to read/write at that address.

---

### 1.3 Memory Allocation in C
**Study:** How `malloc`/`free` work
- `malloc(size)` - allocates bytes on heap, returns `void*`
- `calloc(n, size)` - zero-initialized allocation
- `realloc(ptr, size)` - resize existing allocation
- `free(ptr)` - deallocate, prevent leaks
- Memory fragmentation and allocator internals

**Practice:**
```c
// Allocate array dynamically
int *arr = malloc(100 * sizeof(int));
arr[0] = 42;
free(arr);  // Essential!
```

**Key Insight:** Heap allocation is manual. Forget `free` = leak. Use after `free` = crash.

---

## Phase 2: Vii 1.4 Current Implementation

### 2.1 Stack Allocation Deep Dive
**Study:** What you've built
- `stack_alloc` creates fixed-size arrays
- Pre-allocation with `val_none()`
- `fixed_cap` field in `Value` struct
- Bounds checking in `ND_SET` and `ND_ASSIGN`

**Read:**
- `src/vii.h` - `Value` struct definition
- `src/interp.c` - `ND_STACK_ALLOC` case
- `src/interp.c` - Bounds checking logic

**Practice:**
```vii
# Write tests for edge cases
buf u8[10] = stack_alloc
buf at 0 = 65
buf at 9 = 90
# buf at 10 = 66  # Should error
```

**Key Insight:** `stack_alloc` in Vii creates a list Value with pre-set capacity. The interpreter checks bounds; real stack allocation would use `alloca()` in C.

---

### 2.2 Gradual Typing System
**Study:** How Vii handles types
- Untyped: `x = 5` (inferred as i32)
- Soft typing: `x num = 5` (numeric constraint)
- Hard typing: `x i32 = 5` (exact type)
- `infer_node_type()` in parser
- Type compatibility checking in assignments

**Read:**
- `src/parser.c` - `infer_node_type()` function
- `src/parser.c` - Assignment type checking (lines ~626-640)

**Practice:**
```vii
# Test type compatibility
x i8 = 0      # Should work
y f64 = 3.14  # Should work
z i32 = 5.5   # Should error (float to int)
```

**Key Insight:** Type checking happens at parse time. The parser compares `expected` type with `actual` inferred type.

---

## Phase 3: Pointers and Heap (Next Steps)

### 3.1 Pointer Semantics
**Study:** What `ptr T` really means
- Typed pointer: address + type information
- `addr x` - get address of variable
- `val p` - read value at address (dereference)
- Pointer validity (dangling pointers, use-after-free)
- Pointer arithmetic safety

**Design Questions for Vii:**
- Should `ptr` store type info at runtime or compile time?
- How to handle `val p` when `p` is invalid?
- Should pointer arithmetic be allowed? (`p + 1`)

**Practice:**
```c
// Study unsafe patterns
int *p = malloc(sizeof(int));
free(p);
*p = 10;  // Use-after-free! CRASH
```

---

### 3.2 Heap Allocation Design
**Study:** How to add `heap_alloc` to Vii
- Return type: `ptr T` (typed pointer)
- Manual lifetime management
- Memory leak detection (debug mode)
- Double-free protection

**Design for Vii:**
```vii
# Proposed syntax
buf ptr u8 = heap_alloc 1024   # Allocate
# ... use buf ...
heap_free buf                  # Deallocate
```

**Key Challenge:** Without a borrow checker (like Rust), users can:
- Forget to free (leak)
- Free then use (crash)
- Free twice (crash)

**Possible Solutions:**
1. **Debug tracking:** Track allocations, warn on leaks
2. **Smart pointers:** Reference counting (simpler than borrow checker)
3. **Arena allocation:** Bulk free, harder to leak

---

### 3.3 Function Return Types with Pointers
**Study:** Returning allocated memory from functions
```vii
# Current syntax
do create_buffer size -> ptr u8
    buf ptr u8 = heap_alloc size
    return buf  # Returns pointer, not stack data
```

**Key Rule:** Never return pointers to stack data (use-after-return bug)
```c
// WRONG - classic bug
int* bad() {
    int x = 10;
    return &x;  // x is destroyed on return!
}
```

---

## Phase 4: Advanced Concepts (Future)

### 4.1 Custom Allocators
**Study:** Arena/Pool allocation
- Arena: Pre-allocate large block, bump pointer for allocations
- `arena_reset` frees everything at once (O(1))
- No individual frees, no fragmentation

**Use Case:** Game frames, request handlers
```vii
arena "frame" 16*1024*1024  # 16MB arena

# Each frame
buf = arena.alloc "frame" 1024
# ... use ...
arena.reset "frame"  # Free all at once
```

---

### 4.2 Type Layout and Alignment
**Study:** How types are laid out in memory
- `sizeof(T)` - size in bytes
- `alignof(T)` - alignment requirement
- Padding and packed structs
- Cache lines (64 bytes typical)

**Practice:**
```c
#include <stdio.h>
struct Example {
    char a;      // 1 byte + 3 padding
    int b;       // 4 bytes
    char c;      // 1 byte + 3 padding
};
// sizeof(struct Example) = 12, not 6!
```

**Vii Application:**
```vii
# Future feature
size = sizeof i32     # 4
align = alignof f64   # 8
```

---

### 4.3 Unsafe Blocks
**Study:** When to bypass safety
- Raw pointer arithmetic
- Memory-mapped I/O
- FFI (Foreign Function Interface)
- Volatile memory access

**Design for Vii:**
```vii
safe
    # Normal Vii code, all checks enabled
    arr at 0 = 10  # Bounds checked

unsafe
    # Raw memory access
    ptr = addr arr
    ptr = ptr + 4   # Pointer arithmetic
    val ptr = 20    # No bounds check
```

---

## Study Schedule

| Week | Focus | Deliverable |
|------|-------|-------------|
| 1 | C pointers & malloc | Working C programs with pointers |
| 2 | Memory layout visualization | Diagram stack/heap interaction |
| 3 | Review Vii `stack_alloc` implementation | Explain every line of bounds checking |
| 4 | Design `heap_alloc` API | Documented proposal |
| 5 | Implement minimal `heap_alloc` | Working prototype |
| 6 | Add safety features (leak tracking) | Debug mode allocator |

---

## Resources

### Books
- **"Expert C Programming"** by Peter van der Linden (Chapters 1-4)
- **"C Programming: A Modern Approach"** by K.N. King (Chapters 11, 17)
- **"Computer Systems: A Programmer's Perspective"** by Bryant & O'Hallaron (Chapters 9)

### Online
- [Memory Allocation Strategies](https://www.gingerbill.org/article/2019/02/01/memory-allocation-strategies-001/) - Ginger Bill's series
- [What every programmer should know about memory](https://lwn.net/Articles/250967/) - LWN deep dive
- [Visualizing C Memory Layout](https://www.geeksforgeeks.org/memory-layout-of-c-program/) - Visual guide

### Practice Projects
1. **Write a custom allocator** - Implement a simple bump allocator in C
2. **Pointer visualization tool** - Print memory addresses of stack/heap data
3. **Leak detector** - Track malloc/free pairs, report unmatched allocations
4. **Vii heap prototype** - Add `heap_alloc`/`heap_free` to Vii interpreter

---

## Checklist: Ready for Heap Implementation?

- [ ] Can explain difference between stack and heap with diagrams
- [ ] Written C programs using `malloc`/`free` without leaks (verified with valgrind)
- [ ] Understand pointer arithmetic and alignment
- [ ] Can trace through Vii's `stack_alloc` implementation line by line
- [ ] Designed error handling for use-after-free and double-free
- [ ] Documented proposed `heap_alloc` API with examples

**Don't implement until all checked.**

---

*Vii 1.4 is the foundation. Master these concepts before building 1.5.*
