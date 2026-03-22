#!/usr/bin/env bash
set -euo pipefail

run_with_timeout() {
  local seconds="$1"
  shift

  if command -v timeout >/dev/null 2>&1; then
    timeout "${seconds}s" "$@"
    return
  fi

  if command -v gtimeout >/dev/null 2>&1; then
    gtimeout "${seconds}s" "$@"
    return
  fi

  if command -v python3 >/dev/null 2>&1; then
    python3 -c '
import subprocess
import sys

timeout = int(sys.argv[1])
cmd = sys.argv[2:]
if not cmd:
    raise SystemExit(2)
try:
    proc = subprocess.Popen(cmd)
    proc.wait(timeout=timeout)
    raise SystemExit(proc.returncode)
except subprocess.TimeoutExpired:
    proc.kill()
    proc.wait()
    print("Command timed out after {}s".format(timeout), file=sys.stderr)
    raise SystemExit(124)
' "$seconds" "$@"
    return
  fi

  echo "warning: no timeout utility found; running without timeout" >&2
  "$@"
}

echo "Testing chess engine..."
echo "Testing UCI startup and readiness..."

uci_input=$'uci\nisready\nquit\n'
output="$(printf '%s' "$uci_input" | run_with_timeout 30 bazel run //:engine_uci 2>&1)"
echo "$output"

if ! grep -q "uciok" <<<"$output"; then
  echo "Engine did not return uciok." >&2
  exit 1
fi

if ! grep -q "readyok" <<<"$output"; then
  echo "Engine did not return readyok." >&2
  exit 1
fi

echo "Testing UCI search path..."
search_output="$(
  {
    printf '%s' $'uci\nsetoption name OwnBook value false\nisready\nposition startpos\ngo movetime 200\n'
    sleep 2
    printf 'quit\n'
  } | run_with_timeout 30 bazel run //:engine_uci 2>&1
)"
echo "$search_output"

if ! grep -q "bestmove " <<<"$search_output"; then
  echo "Engine did not return a bestmove in search mode." >&2
  exit 1
fi

echo "Test completed successfully."
