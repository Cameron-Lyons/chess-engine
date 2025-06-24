#!/bin/bash

# Chess Engine Development Setup Script

set -e

echo "🏗️  Setting up Chess Engine development environment..."

# Detect OS
OS=$(uname -s)
echo "Detected OS: $OS"

# Install dependencies based on OS
case $OS in
    "Darwin")
        echo "📦 Installing dependencies for macOS..."
        if ! command -v brew &> /dev/null; then
            echo "❌ Homebrew not found. Please install it first: https://brew.sh"
            exit 1
        fi
        brew install cmake ninja clang-format cppcheck
        ;;
    "Linux")
        echo "📦 Installing dependencies for Linux..."
        sudo apt-get update
        sudo apt-get install -y build-essential cmake ninja-build clang-format clang-tidy cppcheck
        ;;
    *)
        echo "❌ Unsupported OS: $OS"
        exit 1
        ;;
esac

# Create build directory
echo "📁 Creating build directory..."
mkdir -p build

# Setup git hooks
echo "🔧 Setting up git hooks..."
mkdir -p .git/hooks

cat > .git/hooks/pre-commit << 'EOF'
#!/bin/bash
# Pre-commit hook: Format code and run basic checks

echo "🎨 Formatting code..."
find src tests -name "*.cpp" -o -name "*.h" | xargs clang-format -i

echo "🔍 Running static analysis..."
if command -v cppcheck &> /dev/null; then
    cppcheck --enable=all --std=c++23 --suppress=missingIncludeSystem src/ 2>/dev/null || true
fi

echo "✅ Pre-commit checks completed"
EOF

chmod +x .git/hooks/pre-commit

# Create VS Code configuration
echo "⚙️  Creating VS Code configuration..."
mkdir -p .vscode

cat > .vscode/settings.json << 'EOF'
{
    "C_Cpp.default.cppStandard": "c++23",
    "C_Cpp.default.compilerPath": "/usr/bin/g++",
    "C_Cpp.default.includePath": [
        "${workspaceFolder}/src",
        "${workspaceFolder}/include"
    ],
    "files.associations": {
        "*.h": "cpp",
        "*.cpp": "cpp"
    },
    "editor.formatOnSave": true,
    "C_Cpp.clang_format_fallbackStyle": "{ BasedOnStyle: Google, IndentWidth: 4, ColumnLimit: 100 }"
}
EOF

cat > .vscode/tasks.json << 'EOF'
{
    "version": "2.0.0",
    "tasks": [
        {
            "label": "Build Debug",
            "type": "shell",
            "command": "cmake",
            "args": ["--build", "build", "--config", "Debug"],
            "group": "build",
            "presentation": {
                "echo": true,
                "reveal": "always",
                "focus": false,
                "panel": "shared"
            }
        },
        {
            "label": "Build Release",
            "type": "shell",
            "command": "cmake",
            "args": ["--build", "build", "--config", "Release"],
            "group": "build"
        },
        {
            "label": "Run Tests",
            "type": "shell",
            "command": "ctest",
            "args": ["--test-dir", "build", "--output-on-failure"],
            "group": "test"
        }
    ]
}
EOF

cat > .vscode/launch.json << 'EOF'
{
    "version": "0.2.0",
    "configurations": [
        {
            "name": "Debug Chess Engine",
            "type": "cppdbg",
            "request": "launch",
            "program": "${workspaceFolder}/build/chess_engine",
            "args": [],
            "stopAtEntry": false,
            "cwd": "${workspaceFolder}",
            "environment": [],
            "externalConsole": false,
            "MIMode": "gdb",
            "setupCommands": [
                {
                    "description": "Enable pretty-printing for gdb",
                    "text": "-enable-pretty-printing",
                    "ignoreFailures": true
                }
            ]
        }
    ]
}
EOF

# Create clang-format configuration
echo "🎨 Creating code formatting configuration..."
cat > .clang-format << 'EOF'
BasedOnStyle: Google
IndentWidth: 4
ColumnLimit: 100
AllowShortFunctionsOnASingleLine: Empty
AllowShortIfStatementsOnASingleLine: false
AllowShortLoopsOnASingleLine: false
BreakBeforeBraces: Attach
EOF

echo "✅ Development environment setup complete!"
echo ""
echo "Next steps:"
echo "1. Run 'cmake -B build -DCMAKE_BUILD_TYPE=Debug' to configure"
echo "2. Run 'cmake --build build' to build"
echo "3. Run 'ctest --test-dir build' to run tests"
echo "4. Open the project in VS Code for enhanced development experience" 