#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

run_format=1
run_tidy=0
run_test=1
run_sanitize=0
run_dead_code=0

usage() {
    cat <<'EOF'
Usage: scripts/check.sh [OPTIONS]

Run local checks aligned with CI.

Options:
  --full          Run all CI checks (format, tidy, tests, dead-code)
  --format        Run clang-format only
  --tidy          Run clang-tidy only
  --test          Run bazel tests only
  --sanitize      Run bazel tests with ASan+UBSan (skips gtest_perft)
  --dead-code     Run dead-code checks only
  --skip-format   Skip clang-format
  --skip-test     Skip bazel tests
  -h, --help      Show this help

With no options, runs clang-format and bazel tests (fast local default).
Use --full for CI parity.
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --full)
            run_format=1
            run_tidy=1
            run_test=1
            run_sanitize=0
            run_dead_code=1
            ;;
        --format)
            run_format=1
            run_tidy=0
            run_test=0
            run_sanitize=0
            run_dead_code=0
            ;;
        --tidy)
            run_format=0
            run_tidy=1
            run_test=0
            run_sanitize=0
            run_dead_code=0
            ;;
        --test)
            run_format=0
            run_tidy=0
            run_test=1
            run_sanitize=0
            run_dead_code=0
            ;;
        --sanitize)
            run_format=0
            run_tidy=0
            run_test=0
            run_sanitize=1
            run_dead_code=0
            ;;
        --dead-code)
            run_format=0
            run_tidy=0
            run_test=0
            run_sanitize=0
            run_dead_code=1
            ;;
        --skip-format) run_format=0 ;;
        --skip-test) run_test=0 ;;
        -h | --help)
            usage
            exit 0
            ;;
        *)
            echo "unknown option: $1" >&2
            usage >&2
            exit 1
            ;;
    esac
    shift
done

bazel_config=()
if [[ "${CI:-}" == "true" ]] ||
    { [[ "$(uname -s)" == "Linux" ]] && [[ "$(uname -m)" == "x86_64" ]]; }; then
    bazel_config=(--config=ci)
    export REFRESH_COMPILE_COMMANDS_BAZEL_ARGS="${REFRESH_COMPILE_COMMANDS_BAZEL_ARGS:---config=ci}"
fi

if [[ "$run_format" -eq 1 ]]; then
    echo "==> clang-format"
    ./scripts/check_clang_format.sh
fi

if [[ "$run_tidy" -eq 1 ]]; then
    echo "==> clang-tidy"
    ./scripts/run_clang_tidy.sh
fi

if [[ "$run_test" -eq 1 ]]; then
    echo "==> bazel test"
    test_args=(
        test //tests/...
        "--jobs=$(getconf _NPROCESSORS_ONLN 2>/dev/null || nproc 2>/dev/null || echo 4)"
        --config=release
        --test_output=errors
    )
    if ((${#bazel_config[@]} > 0)); then
        test_args+=("${bazel_config[@]}")
    fi
    bazel "${test_args[@]}"
fi

if [[ "$run_sanitize" -eq 1 ]]; then
    echo "==> bazel test (ASan+UBSan)"
    sanitize_args=(
        test //tests/...
        "--jobs=$(getconf _NPROCESSORS_ONLN 2>/dev/null || nproc 2>/dev/null || echo 4)"
        --test_tag_filters=-noasan
        --test_output=errors
    )
    if ((${#bazel_config[@]} > 0)); then
        sanitize_args+=("${bazel_config[@]}")
    fi
    sanitize_args+=(--config=sanitize)
    bazel "${sanitize_args[@]}"
fi

if [[ "$run_dead_code" -eq 1 ]]; then
    echo "==> dead-code"
    ./scripts/check_dead_code.sh
fi

echo "All requested checks passed."
