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

if "$CLANG_FORMAT" --dry-run --Werror "${source_files[@]}"; then
    exit 0
fi

echo "Formatting errors detected. Suggested fixes:" >&2
for source_file in "${source_files[@]}"; do
    if ! diff -u "$source_file" -L "$source_file" -L "$source_file (formatted)" \
        "$source_file" <("$CLANG_FORMAT" "$source_file") >/dev/null 2>&1; then
        echo "=== $source_file ===" >&2
        diff -u "$source_file" -L "$source_file" -L "$source_file (formatted)" \
            "$source_file" <("$CLANG_FORMAT" "$source_file") >&2 || true
    fi
done

exit 1
