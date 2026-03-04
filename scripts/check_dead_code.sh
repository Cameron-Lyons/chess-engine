#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ALLOWLIST_FILE="$ROOT_DIR/config/dead_code_allowlist.txt"

cd "$ROOT_DIR"

for cmd in bazel ctags rg; do
    if ! command -v "$cmd" >/dev/null 2>&1; then
        echo "missing required command: $cmd" >&2
        exit 1
    fi
done

echo "[1/2] Building with unused-code warnings"
bazel build \
    //:engine_lib \
    //:engine_cli \
    //:engine_uci \
    //tests:all \
    --cxxopt=-Wall \
    --cxxopt=-Wextra \
    --cxxopt=-Wunused-function \
    --cxxopt=-Wunused-parameter

echo "[2/2] Scanning for low-reference functions"

TMP_TAGS="$(mktemp)"
TMP_FUNCS="$(mktemp)"
TMP_CANDIDATES="$(mktemp)"
TMP_CANDIDATE_KEYS="$(mktemp)"
TMP_ALLOWLIST_KEYS="$(mktemp)"
TMP_NEW_KEYS="$(mktemp)"

cleanup() {
    rm -f "$TMP_TAGS" "$TMP_FUNCS" "$TMP_CANDIDATES" \
          "$TMP_CANDIDATE_KEYS" "$TMP_ALLOWLIST_KEYS" "$TMP_NEW_KEYS"
}
trap cleanup EXIT

find src -type f -name '*.cpp' -print0 | xargs -0 ctags -x 2>/dev/null > "$TMP_TAGS"

awk 'BEGIN{OFS="\t"} $3 ~ /^src\/.*\.cpp$/ && $0 ~ /\(/ {print $1,$3,$2}' "$TMP_TAGS" |
    sort -u > "$TMP_FUNCS"

while IFS=$'\t' read -r name file line; do
    case "$name" in
        main|operator)
            continue
            ;;
    esac

    if [[ "$name" =~ ^[A-Z] ]]; then
        continue
    fi

    refs=$( (rg -w --no-heading -o "$name" src tests benchmarks examples 2>/dev/null || true) |
        wc -l |
        tr -d ' ')

    if [[ "$refs" -le 2 ]]; then
        printf "%s\t%s\t%s\n" "$name" "$file" "$line" >> "$TMP_CANDIDATES"
    fi
done < "$TMP_FUNCS"

if [[ ! -s "$TMP_CANDIDATES" ]]; then
    echo "No low-reference candidates found."
    exit 0
fi

awk -F'\t' '{print $1"\t"$2}' "$TMP_CANDIDATES" | sort -u > "$TMP_CANDIDATE_KEYS"

if [[ -f "$ALLOWLIST_FILE" ]]; then
    grep -v '^[[:space:]]*#' "$ALLOWLIST_FILE" |
        awk -F'\t' 'NF >= 2 && $1 != "" && $2 != "" {print $1"\t"$2}' |
        sort -u > "$TMP_ALLOWLIST_KEYS"
else
    : > "$TMP_ALLOWLIST_KEYS"
fi

comm -23 "$TMP_CANDIDATE_KEYS" "$TMP_ALLOWLIST_KEYS" > "$TMP_NEW_KEYS"

if [[ ! -s "$TMP_NEW_KEYS" ]]; then
    echo "Low-reference scan passed (no new candidates)."
    exit 0
fi

echo "New low-reference candidates detected:" >&2
while IFS=$'\t' read -r name file; do
    line=$(awk -F'\t' -v n="$name" -v f="$file" '$1==n && $2==f {print $3; exit}' "$TMP_CANDIDATES")
    echo "  - $file:$line ($name)" >&2
done < "$TMP_NEW_KEYS"

exit 1
