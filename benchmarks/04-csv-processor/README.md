# CSV Processor: Vii vs Python

## The Task
Read a CSV file, filter records by amount, aggregate by region, calculate statistics, generate a report.

## Side-by-Side

| Aspect | Python | Vii |
|--------|--------|-----|
| **Lines** | 55 | 48 |
| **Imports** | `csv`, `sys`, `datetime` | None |
| **File Reading** | `with open(...) as f:` | `input_file ask` |
| **CSV Parsing** | `csv.DictReader` | `split ","` — explicit |
| **Type Conversion** | `float(row.get('amount', 0))` | `tonum amount_str` |
| **List Comprehensions** | `[float(r.get('amount',0)) for r in filtered]` | `for ... push` — explicit |
| **Lambda Sort** | `sorted(filtered, key=lambda x: ...)` | Manual sort (simpler here) |
| **String Formatting** | `f"${avg_amount:.2f}"` | Just concat with `+` |

## Python Pain Points

```python
# Imports needed for basic tasks
import csv
import sys
from datetime import datetime

# Verbose file handling
with open(input_file, 'r', newline='') as f:
    reader = csv.DictReader(f)
    for row in reader:
        rows.append(row)

# Type conversion everywhere
amount = float(row.get('amount', 0))

# List comprehensions are compact but opaque
amounts = [float(r.get('amount', 0)) for r in filtered]

# Lambda syntax for sorting
sorted_by_amount = sorted(filtered, 
                           key=lambda x: float(x.get('amount', 0)), 
                           reverse=True)[:5]

# String formatting mini-language
f"Average Amount: ${avg_amount:.2f}"
```

## Vii Clarity

```vii
# No imports needed — everything is built-in

# File reading is one call
content = input_file ask

# CSV parsing is explicit (you see exactly what happens)
lines = content split "\n"
header = (lines at 0) split ","

# Type conversion is a function, not a constructor
amount = tonum amount_str

# Loops are explicit (no list comprehensions to decode)
for row in filtered
  all_amounts push amount

# Sorting is manual here (could use a library)
# But the logic is right there in the code

# String building is just concatenation
report = report + "Average Amount: $" + avg_amount + "\n"
```

## Why Vii Wins

1. **Zero Imports**: No `pip install`, no `requirements.txt`, no dependency hell.
2. **Explicit Over Implicit**: Python's `DictReader` hides the parsing. Vii shows it.
3. **No F-strings to Learn**: Just use `+` for concatenation.
4. **Single Executable**: `vii process.vii -o process` → one binary. Python needs the interpreter.
5. **Memory Efficient**: Vii streams data naturally. Python loads everything into dicts.

## Why Python Wins

- **CSV handling**: `csv.DictReader` handles edge cases (quoted fields, newlines)
- **Standard library**: `datetime`, `statistics`, `json` — all battle-tested
- **Ecosystem**: pandas for real data work
- **Formatting**: `:.2f` is nice for number formatting

## The Trade-off

**Use Python when**:
- You need pandas/numpy for serious data work
- CSV has complex edge cases (embedded commas, newlines)
- You're already in a Python environment

**Use Vii when**:
- You want a single deployable binary
- The CSV is simple (no quoted fields)
- You want zero dependencies
- You're sick of `import` statements for basic tasks

## Run It

```bash
# Python version
python3 process-python.py sales.csv report.txt 100.0

# Vii version (interpreted)
vii process.vii sales.csv report.txt 100

# Vii version (compiled)
vii process.vii -o csvprocessor
./csvprocessor sales.csv report.txt 100
```

---

**Winner**: Tie on capability, Vii wins on deployment simplicity. For simple CSV work, Vii's 0 dependencies beats Python's ecosystem.
