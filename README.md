# Chess Engine

Modern C++23 chess engine with:
- Console play mode (play against the engine in your terminal)
- UCI mode (for chess GUIs)
- Bitboard move generation and alpha-beta style search enhancements

## Quick Start (Play Against It)

### 1) Prerequisites
- [Bazel](https://bazel.build/install) or [Bazelisk](https://github.com/bazelbuild/bazelisk)
- A C++ toolchain available to Bazel (Clang/GCC on Linux/macOS, MSVC on Windows)

### 2) Build
```bash
bazel build //:engine_cli
```

### 3) Start the engine (console play mode)
```bash
bazel run //:engine_cli
```

You will play as White, and the engine plays as Black.

### 4) Enter moves
Use algebraic-style input such as:
- `e4`
- `Nf3`
- `exd5`
- `O-O`
- `O-O-O`
- `e8=Q`

Type `quit` or `exit` to end the game.

## UCI Mode (for Chess GUIs)

Run the engine in UCI mode:
```bash
bazel run //:engine_uci
```

If your GUI needs a binary path, build first and use:
```bash
./bazel-bin/engine_uci
```

## Tests

Run all tests:
```bash
bazel test //tests/...
```

Run dead-code checks (unused-warning build + low-reference scan):
```bash
./scripts/check_dead_code.sh
```

## Benchmarks

Build benchmark binaries:
```bash
bazel build //benchmarks:search_benchmark //benchmarks:micro_benchmark //benchmarks:multipv_benchmark
```

Run search throughput benchmark:
```bash
bazel run //benchmarks:search_benchmark -- --rounds=3 --time_ms=2000 --threads=1 --depth=12
```

Run focused microbenchmark:
```bash
bazel run //benchmarks:micro_benchmark -- --iterations=2000000
```

Run threaded MultiPV scaling benchmark:
```bash
bazel run //benchmarks:multipv_benchmark -- --rounds=2 --time_ms=1200 --depth=10 --multipv=3
```

## Useful Targets

- CLI engine binary: `//:engine_cli`
- UCI engine binary: `//:engine_uci`
- Core engine library: `//:engine_lib`
- Search benchmark: `//benchmarks:search_benchmark`
- Micro benchmark: `//benchmarks:micro_benchmark`
- MultiPV benchmark: `//benchmarks:multipv_benchmark`

## Repository Layout

- `/src/core` - board representation, bitboards, piece/move primitives
- `/src/search` - move generation and search
- `/src/evaluation` - evaluation logic (including NNUE-related code)
- `/src/protocol` - UCI protocol implementation
- `/src/ai` - neural network/tablebase related components
- `/tests` - test targets

## Notes

- Use Bazel outputs (`bazel run` or `bazel-bin/engine_cli`/`bazel-bin/engine_uci`) instead of the checked-in `bin/chess_engine` binaries, which may not match your machine architecture.
