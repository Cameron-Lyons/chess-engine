#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CLANG_FORMAT="${CLANG_FORMAT:-clang-format-21}"

cd "$ROOT_DIR"

if ! command -v "$CLANG_FORMAT" >/dev/null 2>&1; then
    echo "missing required command: $CLANG_FORMAT" >&2
    exit 1
fi

dirs=(src tests)
[[ -d benchmarks ]] && dirs+=(benchmarks)
[[ -d examples ]] && dirs+=(examples)

source_files=()
while IFS= read -r -d '' file; do
    source_files+=("$file")
done < <(find "${dirs[@]}" -type f \( -name '*.cpp' -o -name '*.h' \) -print0)

if [[ "${#source_files[@]}" -eq 0 ]]; then
    echo "no C/C++ sources found under src/, tests/, benchmarks/, or examples/" >&2
    exit 1
fi

echo "Checking formatting with $CLANG_FORMAT (${#source_files[@]} file(s))"

jobs="$(getconf _NPROCESSORS_ONLN 2>/dev/null || nproc 2>/dev/null || echo 4)"

if find "${dirs[@]}" -type f \( -name '*.cpp' -o -name '*.h' \) -print0 |
    xargs -0 -P "$jobs" -n 20 "$CLANG_FORMAT" --dry-run --Werror; then
    exit 0
fi

echo "Formatting errors detected. Suggested fixes:" >&2
for source_file in "${source_files[@]}"; do
    if ! "$CLANG_FORMAT" --dry-run --Werror "$source_file" 2>/dev/null; then
        echo "=== $source_file ===" >&2
        formatted="$(mktemp)"
        "$CLANG_FORMAT" "$source_file" > "$formatted"
        diff -u "$source_file" "$formatted" >&2 || true
        rm -f "$formatted"
    fi
done

exit 1
