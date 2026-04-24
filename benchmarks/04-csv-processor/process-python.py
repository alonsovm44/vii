#!/usr/bin/env python3
"""CSV Processor in Python
Reads sales data, filters, aggregates, generates report.
Lines: 55 | Complexity: Medium | Dependencies: csv module
"""

import csv
import sys
from datetime import datetime

def process_csv(input_file, output_file, min_amount):
    # Read data
    rows = []
    try:
        with open(input_file, 'r', newline='') as f:
            reader = csv.DictReader(f)
            for row in reader:
                rows.append(row)
    except FileNotFoundError:
        print(f"Error: File {input_file} not found", file=sys.stderr)
        sys.exit(1)
    
    # Filter: amount > min_amount
    filtered = []
    for row in rows:
        try:
            amount = float(row.get('amount', 0))
            if amount > min_amount:
                filtered.append(row)
        except ValueError:
            continue
    
    # Aggregate by region
    totals = {}
    for row in filtered:
        region = row.get('region', 'Unknown')
        amount = float(row.get('amount', 0))
        if region not in totals:
            totals[region] = 0
        totals[region] += amount
    
    # Calculate stats
    amounts = [float(r.get('amount', 0)) for r in filtered]
    if amounts:
        avg_amount = sum(amounts) / len(amounts)
        max_amount = max(amounts)
        total_records = len(filtered)
    else:
        avg_amount = max_amount = total_records = 0
    
    # Generate report
    report_lines = [
        "Sales Report",
        "============",
        f"Generated: {datetime.now().isoformat()}",
        f"Input: {input_file}",
        "",
        "Summary",
        "-------",
        f"Total Records: {total_records}",
        f"Average Amount: ${avg_amount:.2f}",
        f"Max Amount: ${max_amount:.2f}",
        "",
        "By Region",
        "---------",
    ]
    
    for region, total in sorted(totals.items()):
        report_lines.append(f"{region}: ${total:.2f}")
    
    report_lines.append("")
    report_lines.append("Top Transactions")
    report_lines.append("----------------")
    
    # Top 5 by amount
    sorted_by_amount = sorted(filtered, 
                               key=lambda x: float(x.get('amount', 0)), 
                               reverse=True)[:5]
    for row in sorted_by_amount:
        report_lines.append(f"{row.get('date', 'N/A')}: {row.get('customer', 'N/A')} - ${row.get('amount', 0)}")
    
    # Write output
    report = "\n".join(report_lines)
    with open(output_file, 'w') as f:
        f.write(report)
    
    print(report)
    print(f"\nReport written to: {output_file}")

if __name__ == "__main__":
    input_file = sys.argv[1] if len(sys.argv) > 1 else "sales.csv"
    output_file = sys.argv[2] if len(sys.argv) > 2 else "report.txt"
    min_amount = float(sys.argv[3]) if len(sys.argv) > 3 else 100.0
    
    process_csv(input_file, output_file, min_amount)
