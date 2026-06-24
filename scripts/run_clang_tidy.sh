#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CLANG_TIDY="${CLANG_TIDY:-clang-tidy-21}"
BAZEL="${BAZEL:-bazel}"

cd "$ROOT_DIR"

for cmd in "$CLANG_TIDY" "$BAZEL" python3; do
    if ! command -v "$cmd" >/dev/null 2>&1; then
        echo "missing required command: $cmd" >&2
        exit 1
    fi
done

echo "Generating compile_commands.json"
if [[ -n "${REFRESH_COMPILE_COMMANDS_BAZEL_ARGS:-}" ]]; then
    # shellcheck disable=SC2206
    refresh_bazel_args=($REFRESH_COMPILE_COMMANDS_BAZEL_ARGS)
    "$BAZEL" run //:refresh_compile_commands -- "${refresh_bazel_args[@]}"
else
    "$BAZEL" run //:refresh_compile_commands
fi

compile_commands="$ROOT_DIR/compile_commands.json"
if [[ ! -f "$compile_commands" ]]; then
    echo "compile_commands.json not found after refresh" >&2
    exit 1
fi

source_files=()
while IFS= read -r file; do
    source_files+=("$file")
done < <(
    python3 - "$compile_commands" "${CLANG_TIDY_PATHS:-src,tests}" <<'PY'
import json
import sys

compile_commands_path = sys.argv[1]
prefixes = tuple(
    prefix if prefix.endswith("/") else f"{prefix}/"
    for part in sys.argv[2].split(",")
    if (prefix := part.strip())
)

with open(compile_commands_path, encoding="utf-8") as handle:
    entries = json.load(handle)

files = sorted(
    {
        entry["file"]
        for entry in entries
        if entry["file"].endswith(".cpp") and entry["file"].startswith(prefixes)
    }
)

if not files:
    print(
        f"no C++ sources found for paths {', '.join(prefixes)} in compile_commands.json",
        file=sys.stderr,
    )
    sys.exit(1)

print("\n".join(files))
PY
)

if [[ "${#source_files[@]}" -eq 0 ]]; then
    echo "no C++ sources matched CLANG_TIDY_PATHS=${CLANG_TIDY_PATHS:-src,tests}" >&2
    exit 1
fi

echo "Running $CLANG_TIDY on ${#source_files[@]} translation unit(s)"

jobs="$(getconf _NPROCESSORS_ONLN 2>/dev/null || nproc 2>/dev/null || echo 4)"

printf '%s\0' "${source_files[@]}" | xargs -0 -P "$jobs" -I{} \
    "$CLANG_TIDY" {} \
        --config-file="$ROOT_DIR/.clang-tidy" \
        --warnings-as-errors='*' \
        -p "$ROOT_DIR"

echo "clang-tidy passed"
