#!/bin/bash
# Script to run clang-tidy on all C++ source files

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "Running clang-tidy..."

# Check if clang-tidy is available
if ! command -v clang-tidy &> /dev/null; then
    echo "Error: clang-tidy not found. Please install clang-tidy."
    exit 1
fi

# Create compile_commands.json if it doesn't exist
if [ ! -f "$PROJECT_ROOT/compile_commands.json" ]; then
    echo "compile_commands.json not found. Generating from CMake..."
    cd "$PROJECT_ROOT"
    if [ ! -d "build" ]; then
        mkdir build
    fi
    cd build
    cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ..
    cd ..
    ln -sf build/compile_commands.json compile_commands.json
fi

# Run clang-tidy on all source files
find "$PROJECT_ROOT/src" \
     "$PROJECT_ROOT/tests" \
     "$PROJECT_ROOT/benchmarks" \
     -type f \( -name "*.cpp" -o -name "*.h" -o -name "*.hpp" \) \
     -not -path "*/build/*" \
     -not -path "*/bin/*" \
     | while read -r file; do
    echo "Checking: $file"
    clang-tidy -p "$PROJECT_ROOT" "$file" || true
done

echo "clang-tidy check complete!"

