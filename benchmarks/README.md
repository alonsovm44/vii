# Vii vs The World

Real examples showing why Vii replaces messy shell scripts.

## The Pitch

| Tool | Problem | Vii Solution |
|------|---------|--------------|
| **Bash** | `"$var"` vs `$var` vs `${var}` hell, `set -euo pipefail` just to catch errors | Just write logic. No sigils. No flags. |
| **Make** | Tab-sensitive, cryptic syntax, shell escaping nightmares | Readable steps, real conditionals, strings that just work |
| **Python** | 50 lines of imports and boilerplate for a 5-line task | Zero boilerplate. Implicit output. |
| **YAML+Jinja** | Turing-complete config files that are write-only | Actual language with real error messages |

---

## Examples

1. **[Log Analyzer](01-log-analyzer/)** — Parse logs, extract errors, generate report
2. **[Build Script](02-build-script/)** — Replace Make with readable build steps  
3. **[Deploy Pipeline](03-deploy-pipeline/)** — SSH, docker, health checks
4. **[CSV Processor](04-csv-processor/)** — Data transformation without pandas
5. **[Project Bootstrap](05-project-bootstrap/)** — Scaffold new projects

---

## Running Comparisons

```bash
# See both versions
cd 01-log-analyzer
cat analyze-bash.sh      # The pain
cat analyze.vii          # The relief
./analyze.vii            # Run it

# Compile Vii to binary for speed
vii analyze.vii -o analyze
./analyze
```

---

**Common Theme**: Vii eliminates the " punctuation tax" that makes shell scripts write-only. If you can read it, you can maintain it.
