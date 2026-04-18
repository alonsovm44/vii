
color_red = "\x1b[1;31m"
color_grn = "\x1b[1;32m"
color_yel = "\x1b[1;33m"
color_rst = "\x1b[0m"

# Helper to print errors with colors
do report_error filename line message
  color_red
  "Error: " + filename + " at line " + line
  message
  color_rst
  0 # Return 0 for failure

# The core compiler/interpreter logic
do run_compiler target_file
  source_code = target_file ask
  if source_code == 0
    report_error target_file 0 "Could not read file"
  else
    "Compiling..." + target_file
    # Here you would implement the lexer/parser logic
    # iterating through source_code using 'at'
    1 # Return 1 for success

# Entry Point
target = arg at 0

if target == 0
  color_yel
  "IO Compiler v1.0.0"
  "Usage: io io.io <file.io>"
  color_rst
else
  run_compiler target
