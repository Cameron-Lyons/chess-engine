#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CLANG_TIDY="${CLANG_TIDY:-clang-tidy-21}"

cd "$ROOT_DIR"

if ! command -v "$CLANG_TIDY" >/dev/null 2>&1; then
    echo "missing required command: $CLANG_TIDY" >&2
    exit 1
fi

if ! find src -type f -name '*.cpp' -print -quit | grep -q .; then
    echo "no C++ sources found under src/" >&2
    exit 1
fi

echo "Running $CLANG_TIDY"
find src -type f -name '*.cpp' -print0 | xargs -0 -P "$(nproc)" -I{} \
    "$CLANG_TIDY" {} \
        --config-file="$ROOT_DIR/.clang-tidy" \
        --warnings-as-errors='*' \
        -- \
        -std=c++26 \
        -Isrc \
        -Isrc/core \
        -mavx2 \
        -march=x86-64-v3

echo "clang-tidy passed"
