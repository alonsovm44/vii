# Build Script: Vii vs Make

## The Task
Build a C project: compile sources, link, run tests, install binary.

## Side-by-Side

| Aspect | Make | Vii |
|--------|------|-----|
| **Lines** | 48 | 42 |
| **Tab Sensitivity** | Fatal (spaces = broken) | Indentation is flexible |
| **Conditionals** | Horrible (`ifeq`, `ifdef`) | `if debug_mode == 1` — just works |
| **String Ops** | `$(patsubst)` — arcane | `split`, `+` for concat |
| **Debug Builds** | Target-specific vars: `debug: CFLAGS = ...` | Pass a parameter: `build 1` |
| **Error Handling** | `\|\| { echo; exit 1; }` | `if result != 0` — readable |
| **Help Text** | Not built-in | Easy: just print strings |

## Make Pain Points

```make
# TABS MATTER — this must start with tab, not spaces
$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# String substitution requires memorizing functions
OBJECTS = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SOURCES))

# Conditionals are ugly
ifeq ($(DEBUG),1)
  CFLAGS += -g
endif

# Shell escaping in recipes
test: $(TARGET)
	@./$(TARGET) --test || { echo "Tests failed!"; exit 1; }
```

## Vii Clarity

```vii
# Regular indentation (2 spaces, 4 spaces, tabs — all work)
do build debug_mode
  flags = CFLAGS
  if debug_mode == 1
    flags = DEBUG_CFLAGS

# String operations are obvious
obj_name = OBJDIR + "/" + (name_parts at 0) + ".o"

# Real conditionals
if result != 0
  "Compilation failed!"
  exit 1

# Help is just... printing
if command == "help"
  "Usage: vii build.vii [command]"
  "Commands:"
  "  build   - Build the project"
```

## Why Vii Wins

1. **No Tab Hell**: Make requires tabs. Vii doesn't care about your indentation style.
2. **Real Functions**: `do build debug_mode` — pass parameters naturally.
3. **Readable Logic**: `if result != 0` vs shell `|| { ... }`.
4. **String Concatenation**: Just use `+`. No `patsubst`, `$*`, or `$@` magic.
5. **Help Built-in**: Easy to add documentation.
6. **Compiles to Binary**: `vii build.vii -o build` → ship a build tool.

## What Make Does Better

- **Dependency tracking**: Make only rebuilds changed files automatically
- **Ecosystem**: Everyone has Make
- **Parallel builds**: `make -j4` is built-in

## What Vii Does Better

- **Readable**: You can understand this in 30 seconds
- **Maintainable**: Adding a new build step is obvious
- **Flexible**: Real scripting, not pattern matching
- **Debuggable**: Error messages make sense

## Run It

```bash
# Make version
cd 02-build-script
make                    # Build
make debug             # Debug build  
make test              # Run tests
make clean             # Clean
make install           # Install

# Vii version (interpreted)
vii build.vii build     # Build
vii build.vii debug     # Debug build
vii build.vii test      # Run tests
vii build.vii clean     # Clean
vii build.vii install   # Install
vii build.vii help      # Show help

# Vii version (compiled)
vii build.vii -o build
./build test
```

---

**Winner**: Vii for readability and maintainability. Make for raw speed and dependency tracking. But Vii compiles to a static binary you can ship anywhere.
