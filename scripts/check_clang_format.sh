#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CLANG_FORMAT="${CLANG_FORMAT:-clang-format-21}"

cd "$ROOT_DIR"

if ! command -v "$CLANG_FORMAT" >/dev/null 2>&1; then
    echo "missing required command: $CLANG_FORMAT" >&2
    exit 1
fi

if ! find src tests -type f \( -name '*.cpp' -o -name '*.h' \) -print -quit | grep -q .; then
    echo "no C/C++ sources found under src/ or tests/" >&2
    exit 1
fi

echo "Checking formatting with $CLANG_FORMAT"
find src tests -type f \( -name '*.cpp' -o -name '*.h' \) -print0 | \
    xargs -0 -P "$(nproc)" -n 20 "$CLANG_FORMAT" --dry-run --Werror
