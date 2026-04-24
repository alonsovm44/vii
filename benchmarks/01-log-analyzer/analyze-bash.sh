#!/bin/bash
# Log Analyzer in Bash
# Extracts ERROR lines, counts by type, generates report
# Lines: 45 | Complexity: High | Maintainability: Painful

set -euo pipefail

LOG_FILE="${1:-app.log}"
OUTPUT="${2:-report.txt}"

if [[ ! -f "$LOG_FILE" ]]; then
    echo "Error: File $LOG_FILE not found" >&2
    exit 1
fi

# Count total lines
TOTAL=$(wc -l < "$LOG_FILE" | tr -d ' ')

# Extract errors with timestamps
ERRORS=$(grep -E '^\[.*\] ERROR' "$LOG_FILE" 2>/dev/null || true)
ERROR_COUNT=$(echo "$ERRORS" | grep -c . || echo "0")

# Count by error type (Database, Network, Auth)
DB_COUNT=$(echo "$ERRORS" | grep -c "Database" || echo "0")
NET_COUNT=$(echo "$ERRORS" | grep -c "Network" || echo "0")
AUTH_COUNT=$(echo "$ERRORS" | grep -c "Auth" || echo "0")

# Get most recent 5 errors
RECENT=$(echo "$ERRORS" | tail -n 5)

# Write report
cat > "$OUTPUT" << EOF
Log Analysis Report
===================
Generated: $(date)
File: $LOG_FILE

Summary
-------
Total Lines: $TOTAL
Total Errors: $ERROR_COUNT

Breakdown
---------
Database Errors: $DB_COUNT
Network Errors: $NET_COUNT  
Auth Errors: $AUTH_COUNT

Recent Errors
-------------
$RECENT

EOF

# Also print to stdout
cat "$OUTPUT"

# Exit with error code if errors found
if [[ $ERROR_COUNT -gt 0 ]]; then
    exit 1
fi
