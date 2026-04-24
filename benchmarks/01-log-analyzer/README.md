# Log Analyzer: Vii vs Bash

## The Task
Parse an application log file, extract ERROR entries, categorize them, and generate a report.

## Side-by-Side

| Aspect | Bash | Vii |
|--------|------|-----|
| **Lines** | 45 | 35 |
| **Special Characters** | `$, {, }, [, ], \|, <, >, -, _, (, ), ;, &` | `+, =, >, <` |
| **Escape Risk** | `"$var"` vs `$var` vs `${var}` — all different! | Just write the name |
| **Error Handling** | `set -euo pipefail` + `\|\| true` + `>&2` | Ask returns "" on missing file |
| **Comments** | Only at line start | Anywhere with `#` |

## Bash Pain Points

```bash
# Variable quoting hell
LOG_FILE="${1:-app.log}"          # Default values need syntax
wc -l < "$LOG_FILE" | tr -d ' '   # Pipe needed to strip whitespace
echo "$ERRORS" | grep -c .        # echo + pipe just to count

# Error suppression needed or script dies
grep -E '^\[.*\] ERROR' "$LOG_FILE" 2>/dev/null || true

# Here-doc with escaped variables
cat > "$OUTPUT" << EOF
File: $LOG_FILE                    # Did I quote this right?
EOF
```

## Vii Clarity

```vii
# Arguments with defaults
log_file = arg at 1
if log_file == ""
  log_file = "app.log"

# Read file — fails gracefully
content = log_file ask

# String concatenation with +
report = "File: " + log_file + "\n"

# List operations are methods
errors push line
error_count = errors len
```

## Why Vii Wins

1. **No Quoting Anxiety**: In Bash, unquoted variables split on spaces. In Vii, strings are strings.
2. **Built-in Data Structures**: Lists have `push`, `len`, `at`. No `wc`, `awk`, `grep` needed.
3. **Readable Conditionals**: `if line contains "ERROR"` vs `grep -E` with regex escapes.
4. **Implicit Output**: Just write `report` at the end — it prints. No `echo`, no `cat`, no redirection.
5. **Compiles to Binary**: Run `vii analyze.vii -o analyze` → get a static executable.

## Run It

```bash
# Bash version
chmod +x analyze-bash.sh
./analyze-bash.sh server.log report.txt

# Vii version (interpreted)
vii analyze.vii server.log report.txt

# Vii version (compiled)
vii analyze.vii -o analyze
./analyze server.log report.txt
```

---

**Winner**: Vii by knockout. 35 readable lines vs 45 lines of punctuation soup.
