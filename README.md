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
bazel build //:chess_engine
```

### 3) Start the engine (console play mode)
```bash
bazel run //:chess_engine
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
bazel run //:chess_engine -- uci
```

If your GUI needs a binary path, build first and use:
```bash
./bazel-bin/chess_engine uci
```

## Tests

Run all tests:
```bash
bazel test //tests/...
```

## Benchmarks

Build benchmark binaries:
```bash
bazel build //benchmarks:search_benchmark //benchmarks:micro_benchmark
```

Run search throughput benchmark:
```bash
bazel run //benchmarks:search_benchmark -- --rounds=3 --time_ms=2000 --threads=1 --depth=12
```

Run focused microbenchmark:
```bash
bazel run //benchmarks:micro_benchmark -- --iterations=2000000
```

## Useful Targets

- Engine binary: `//:chess_engine`
- Core engine library: `//:engine_lib`
- Search benchmark: `//benchmarks:search_benchmark`
- Micro benchmark: `//benchmarks:micro_benchmark`

## Repository Layout

- `/src/core` - board representation, bitboards, piece/move primitives
- `/src/search` - move generation and search
- `/src/evaluation` - evaluation logic (including NNUE-related code)
- `/src/protocol` - UCI protocol implementation
- `/src/ai` - neural network/tablebase related components
- `/tests` - test targets

## Notes

- Use Bazel outputs (`bazel run` or `bazel-bin/chess_engine`) instead of the checked-in `bin/chess_engine` binaries, which may not match your machine architecture.
