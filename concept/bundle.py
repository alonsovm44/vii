import os
import sys
import argparse

def amalgamate_c_files(source_dir, output_file, keep_local_includes=False):
    # Get all .c files in the directory and sort them for consistent output
    c_files = sorted([f for f in os.listdir(source_dir) if f.endswith('.c')])

    if not c_files:
        print(f"No .c files found in '{source_dir}'")
        return

    with open(output_file, 'w') as outfile:
        # Write a header to the new file
        outfile.write("/*\n * AMALGAMATED SOURCE FILE\n")
        outfile.write(f" * Generated from directory: {os.path.abspath(source_dir)}\n")
        outfile.write(f" * Files included: {len(c_files)}\n */\n\n")

        for filename in c_files:
            filepath = os.path.join(source_dir, filename)
            
            print(f"Processing: {filename}")
            
            # Write a separator banner
            outfile.write(f"/*============================================================\n")
            outfile.write(f" * FILE: {filename}\n")
            outfile.write(f" *============================================================*/\n\n")

            with open(filepath, 'r') as infile:
                for line in infile:
                    # Logic to handle #include directives
                    if line.strip().startswith('#include'):
                        # Check if it's a local include (wrapped in quotes)
                        if '"' in line and not keep_local_includes:
                            # Comment out the local include to prevent duplicate definitions
                            outfile.write(f"// [AMALGAMATOR] Removed local include: {line.strip()}\n")
                            continue
                    
                    # Write the line as-is
                    outfile.write(line)
            
            outfile.write("\n\n") # Add spacing between files

    print(f"\nSuccess! Amalgamated {len(c_files)} files into '{output_file}'")

if __name__ == "__main__":
    # Set up command line arguments
    parser = argparse.ArgumentParser(description="Amalgamate all .c files in a directory into a single file.")
    parser.add_argument("source_dir", help="Directory containing the .c files")
    parser.add_argument("output_file", help="Name of the final amalgamated .c file")
    parser.add_argument("--keep-includes", action="store_true", help="Keep local #include \"...\" lines (Not recommended)")
    
    args = parser.parse_args()

    # Validate source directory
    if not os.path.isdir(args.source_dir):
        print(f"Error: '{args.source_dir}' is not a valid directory.")
        sys.exit(1)

    amalgamate_c_files(args.source_dir, args.output_file, args.keep_includes)