#!/bin/bash
# Script to format all C++ source files using clang-format

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Check if clang-format is available
if ! command -v clang-format &> /dev/null; then
    echo "Error: clang-format not found. Please install clang-format."
    exit 1
fi

echo "Formatting code with clang-format..."
echo "Project root: $PROJECT_ROOT"

# Find all C++ source files
find "$PROJECT_ROOT/src" \
     "$PROJECT_ROOT/tests" \
     "$PROJECT_ROOT/benchmarks" \
     -type f \( -name "*.cpp" -o -name "*.h" -o -name "*.hpp" \) \
     -not -path "*/build/*" \
     -not -path "*/bin/*" \
     | while read -r file; do
    echo "Formatting: $file"
    clang-format -i -style=file "$file"
done

echo "Code formatting complete!"

