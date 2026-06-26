# Chess Engine

Modern C++26 chess engine with:
- Console play mode (play against the engine in your terminal)
- UCI mode (for chess GUIs)
- Bitboard move generation with magic bitboards
- Alpha-beta search with iterative deepening, transposition table, LMR, and Lazy SMP
- Hybrid evaluation (traditional heuristics + optional NNUE)
- Syzygy endgame tablebase probing

Built with Bazel (bzlmod), `-Werror`, and C++26 (`-std=c++26` in `.bazelrc`).

## Quick Start (Play Against It)

### 1) Prerequisites
- [Bazel](https://bazel.build/install) or [Bazelisk](https://github.com/bazelbuild/bazelisk)
- A C++26-capable toolchain available to Bazel (Clang/GCC on Linux/macOS, MSVC on Windows)

### 2) Build
```bash
bazel build //:engine_cli //:engine_uci
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

## CLI Modes

`engine_cli` defaults to interactive play. Additional subcommands:

| Command | Description |
|---------|-------------|
| `bazel run //:engine_cli -- analyze [fen]` | Print a detailed position analysis |
| `bazel run //:engine_cli -- train [games]` | Self-play NN training (default 100 games) |
| `bazel run //:engine_cli -- test` | Evaluate sample positions with traditional, NN, and hybrid eval |
| `bazel run //:engine_cli -- generate [games] [path]` | Generate self-play training data |
| `bazel run //:engine_cli -- tune <positions_file> [iterations]` | Texel-style evaluation tuning |

UCI is provided by the separate `engine_uci` binary (see below).

## UCI Mode (for Chess GUIs)

Run the engine in UCI mode:
```bash
bazel run //:engine_uci
```

If your GUI needs a binary path, build first and use:
```bash
./bazel-bin/engine_uci
```

Supported UCI options include `Hash`, `Threads`, `MultiPV`, `Use Neural Network`, `EvalFile`, `Use Tablebases`, and `SyzygyPath`. Set `SyzygyPath` to your Syzygy `.rtbw`/`.rtbz` directory to enable tablebase probing.

Quick smoke test:
```bash
./test_engine.sh
```

## Tests

Run all tests:
```bash
bazel test //tests/...
```

Run the same checks as CI (format, tidy, tests, dead-code):
```bash
./scripts/check.sh --full
```

For a faster local loop (format + tests):
```bash
./scripts/check.sh
```

Optional pre-commit hook for formatting:
```bash
pip install pre-commit
pre-commit install
```

Sanitizer tests (ASan+UBSan; skips `gtest_perft`):
```bash
./scripts/check.sh --sanitize
```

Run dead-code checks (unused-warning build + low-reference scan):
```bash
./scripts/check_dead_code.sh
```

CI runs on Ubuntu 24.04 with GCC 14 and Clang 21.

## Benchmarks

Build benchmark binaries:
```bash
bazel build //benchmarks:search_benchmark //benchmarks:micro_benchmark \
  //benchmarks:multipv_benchmark //benchmarks:lazy_smp_benchmark \
  //benchmarks:perft_benchmark //benchmarks:search_probe
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

Run Lazy SMP scaling benchmark:
```bash
bazel run //benchmarks:lazy_smp_benchmark
```

Run perft benchmark:
```bash
bazel run //benchmarks:perft_benchmark
```

## Useful Targets

- CLI engine binary: `//:engine_cli`
- UCI engine binary: `//:engine_uci`
- Core engine library: `//:engine_lib`
- Endgame example: `//examples:endgame_example`
- Search benchmark: `//benchmarks:search_benchmark`
- Micro benchmark: `//benchmarks:micro_benchmark`
- MultiPV benchmark: `//benchmarks:multipv_benchmark`
- Lazy SMP benchmark: `//benchmarks:lazy_smp_benchmark`
- Perft benchmark: `//benchmarks:perft_benchmark`
- Search probe: `//benchmarks:search_probe`

## IDE Support

Generate `compile_commands.json` for clangd and other tools:
```bash
bazel run //:refresh_compile_commands
```

## Repository Layout

- `/src/core` — board representation, bitboards, piece/move primitives
- `/src/search` — move generation, search, transposition table, opening book
- `/src/evaluation` — traditional evaluation, NNUE, hybrid evaluator, tuning
- `/src/protocol` — UCI protocol implementation
- `/src/ai` — neural network and tablebase components
- `/src/utils` — formatting, globals, search threading helpers
- `/tests` — GoogleTest targets
- `/benchmarks` — search and micro-benchmark binaries
- `/examples` — standalone example programs
- `/scripts` — CI checks, tuning utilities, benchmark helpers
- `/config` — dead-code allowlist and related config

See [`src/README.md`](src/README.md) for a module-level overview of the source tree.

## Notes

- Always use Bazel outputs (`bazel run` or `./bazel-bin/engine_cli` / `./bazel-bin/engine_uci`); there are no checked-in prebuilt binaries.
