#!/bin/bash
# Project Bootstrap in Bash
# Creates a new project structure from template
# Lines: 78 | Complexity: High | Error-prone

set -euo pipefail

# Get project name
PROJECT_NAME="${1:-}"
if [[ -z "$PROJECT_NAME" ]]; then
    echo "Usage: $0 <project-name> [template]"
    echo "Templates: python, node, go, rust"
    exit 1
fi

TEMPLATE="${2:-python}"
PROJECT_DIR="./$PROJECT_NAME"

# Check if directory exists
if [[ -d "$PROJECT_DIR" ]]; then
    echo "Error: Directory $PROJECT_DIR already exists"
    exit 1
fi

echo "Creating $TEMPLATE project: $PROJECT_NAME"

# Create directory structure
mkdir -p "$PROJECT_DIR/src"
mkdir -p "$PROJECT_DIR/tests"
mkdir -p "$PROJECT_DIR/docs"

# Template-specific files
case "$TEMPLATE" in
    python)
        # Python template
        cat > "$PROJECT_DIR/main.py" << 'EOF'
def main():
    print("Hello from {{PROJECT_NAME}}!")

if __name__ == "__main__":
    main()
EOF
        
        cat > "$PROJECT_DIR/requirements.txt" << 'EOF'
# Add dependencies here
EOF
        
        cat > "$PROJECT_DIR/setup.py" << EOF
from setuptools import setup, find_packages

setup(
    name="$PROJECT_NAME",
    version="0.1.0",
    packages=find_packages(),
)
EOF
        
        cat > "$PROJECT_DIR/Makefile" << 'EOF'
.PHONY: install test lint

install:
	pip install -r requirements.txt

test:
	pytest tests/

lint:
	flake8 src/
EOF
        ;;
        
    node)
        # Node.js template
        cat > "$PROJECT_DIR/package.json" << EOF
{
  "name": "$PROJECT_NAME",
  "version": "0.1.0",
  "description": "",
  "main": "index.js",
  "scripts": {
    "test": "echo \"Error: no test specified\" && exit 1"
  }
}
EOF
        
        cat > "$PROJECT_DIR/index.js" << 'EOF'
console.log("Hello from {{PROJECT_NAME}}!");
EOF
        
        cat > "$PROJECT_DIR/.gitignore" << 'EOF'
node_modules/
*.log
EOF
        ;;
        
    go)
        # Go template
        cat > "$PROJECT_DIR/go.mod" << EOF
module github.com/user/$PROJECT_NAME

go 1.21
EOF
        
        cat > "$PROJECT_DIR/main.go" << 'EOF'
package main

import "fmt"

func main() {
    fmt.Println("Hello from {{PROJECT_NAME}}!")
}
EOF
        ;;
        
    rust)
        # Rust template - just use cargo
        echo "Use 'cargo new $PROJECT_NAME' for Rust projects"
        rm -rf "$PROJECT_DIR"
        exit 0
        ;;
        
    *)
        echo "Unknown template: $TEMPLATE"
        echo "Available: python, node, go, rust"
        rm -rf "$PROJECT_DIR"
        exit 1
        ;;
esac

# Common files for all templates
cat > "$PROJECT_DIR/README.md" << EOF
# $PROJECT_NAME

Created with template: $TEMPLATE

## Getting Started

\`\`\`bash
# Add setup instructions here
\`\`\`
EOF

cat > "$PROJECT_DIR/.gitignore" << 'EOF'
# Build artifacts
build/
dist/
*.egg-info/
__pycache__/
.node_modules/
EOF

# Initialize git
cd "$PROJECT_DIR"
if command -v git &> /dev/null; then
    git init
    git add .
    git commit -m "Initial commit"
    echo "Git repository initialized"
fi

# Success message
echo ""
echo "✓ Project created: $PROJECT_DIR"
echo "✓ Template: $TEMPLATE"
echo "✓ Git: initialized"
echo ""
echo "Next steps:"
echo "  cd $PROJECT_NAME"
case "$TEMPLATE" in
    python) echo "  make install" ;;
    node) echo "  npm install" ;;
    go) echo "  go run ." ;;
esac
