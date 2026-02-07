#!/usr/bin/env python3
"""SPSA tuning framework for chess engine parameters using cutechess-cli."""

import argparse
import json
import math
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path


@dataclass
class SPSAParam:
    """A parameter being tuned via SPSA."""

    name: str
    value: float
    min_val: float
    max_val: float
    step: float
    c_end: float = 0.0
    r: float = 0.0

    def __post_init__(self) -> None:
        self.c_end = self.step / 2.0
        self.r = (self.max_val - self.min_val) / 20.0


def parse_uci_options(engine_path: str) -> list[SPSAParam]:
    """Extract tunable parameters from engine's UCI output."""
    params: list[SPSAParam] = []
    proc = subprocess.run(
        [engine_path],
        input="uci\nquit\n",
        capture_output=True,
        text=True,
        timeout=5,
    )

    for line in proc.stdout.splitlines():
        if "option name" not in line or "type spin" not in line:
            continue
        parts = line.split()
        try:
            name_idx = parts.index("name") + 1
            type_idx = parts.index("type")
            name = " ".join(parts[name_idx:type_idx])
            default = int(parts[parts.index("default") + 1])
            min_val = int(parts[parts.index("min") + 1])
            max_val = int(parts[parts.index("max") + 1])
        except (ValueError, IndexError):
            continue

        step = max(1, (max_val - min_val) // 20)
        params.append(
            SPSAParam(
                name=name,
                value=float(default),
                min_val=float(min_val),
                max_val=float(max_val),
                step=float(step),
            )
        )

    return params


def run_match(
    engine_path: str,
    params_plus: dict[str, int],
    params_minus: dict[str, int],
    games: int,
    time_control: str,
    concurrency: int,
    cutechess_path: str,
) -> tuple[float, float]:
    """Run a match between two parameter configurations and return scores."""
    plus_options = " ".join(f"option.{k}={v}" for k, v in params_plus.items())
    minus_options = " ".join(f"option.{k}={v}" for k, v in params_minus.items())

    cmd = [
        cutechess_path,
        "-engine",
        f"cmd={engine_path}",
        f"{plus_options}",
        "name=plus",
        "-engine",
        f"cmd={engine_path}",
        f"{minus_options}",
        "name=minus",
        "-each",
        f"tc={time_control}",
        "proto=uci",
        "-games",
        str(games),
        "-concurrency",
        str(concurrency),
        "-pgnout",
        "spsa_games.pgn",
        "-recover",
    ]

    proc = subprocess.run(cmd, capture_output=True, text=True, timeout=3600)

    wins_plus = 0.0
    wins_minus = 0.0
    draws = 0.0

    for line in proc.stdout.splitlines():
        if "Score of plus vs minus" in line:
            parts = line.split(":")[-1].strip().split("-")
            if len(parts) >= 3:
                wins_plus = float(parts[0].strip())
                wins_minus = float(parts[2].strip().split()[0])
                draws = float(parts[1].strip())
            break

    total = wins_plus + wins_minus + draws
    if total == 0:
        return 0.5, 0.5

    score_plus = (wins_plus + draws * 0.5) / total
    score_minus = (wins_minus + draws * 0.5) / total
    return score_plus, score_minus


def spsa_iteration(
    params: list[SPSAParam],
    iteration: int,
    a: float,
    c: float,
    alpha: float,
    gamma: float,
    engine_path: str,
    games_per_iter: int,
    time_control: str,
    concurrency: int,
    cutechess_path: str,
) -> None:
    """Perform one SPSA iteration."""
    ak = a / (iteration + 1) ** alpha
    ck = c / (iteration + 1) ** gamma

    delta = {}
    params_plus = {}
    params_minus = {}

    for p in params:
        d = 1 if hash(f"{p.name}{iteration}") % 2 == 0 else -1
        delta[p.name] = d

        plus_val = max(p.min_val, min(p.max_val, p.value + ck * d))
        minus_val = max(p.min_val, min(p.max_val, p.value - ck * d))
        params_plus[p.name] = int(round(plus_val))
        params_minus[p.name] = int(round(minus_val))

    score_plus, score_minus = run_match(
        engine_path,
        params_plus,
        params_minus,
        games_per_iter,
        time_control,
        concurrency,
        cutechess_path,
    )

    gradient_estimate = (score_plus - score_minus) / (2.0 * ck)

    for p in params:
        g = gradient_estimate * delta[p.name]
        p.value += ak * g
        p.value = max(p.min_val, min(p.max_val, p.value))


def main() -> None:
    """Run the SPSA tuning loop."""
    parser = argparse.ArgumentParser(description="SPSA tuning for chess engine")
    parser.add_argument("--engine", required=True, help="Path to engine binary")
    parser.add_argument(
        "--cutechess", default="cutechess-cli", help="Path to cutechess-cli"
    )
    parser.add_argument(
        "--iterations", type=int, default=500, help="Number of SPSA iterations"
    )
    parser.add_argument(
        "--games", type=int, default=100, help="Games per iteration pair"
    )
    parser.add_argument("--tc", default="10+0.1", help="Time control")
    parser.add_argument("--concurrency", type=int, default=4, help="Concurrency")
    parser.add_argument("--output", default="spsa_result.json", help="Output file")
    parser.add_argument("--params", nargs="*", help="Specific parameters to tune")
    parser.add_argument(
        "--a", type=float, default=0.02, help="SPSA 'a' parameter (step size)"
    )
    parser.add_argument(
        "--c", type=float, default=2.0, help="SPSA 'c' parameter (perturbation size)"
    )
    parser.add_argument("--alpha", type=float, default=0.602, help="SPSA alpha")
    parser.add_argument("--gamma", type=float, default=0.101, help="SPSA gamma")
    args = parser.parse_args()

    params = parse_uci_options(args.engine)
    if not params:
        print("No tunable parameters found in engine UCI output.")
        sys.exit(1)

    if args.params:
        params = [p for p in params if p.name in args.params]
        if not params:
            print(f"None of the specified parameters found: {args.params}")
            sys.exit(1)

    print(f"Tuning {len(params)} parameters:")
    for p in params:
        print(f"  {p.name}: {p.value} [{p.min_val}, {p.max_val}] step={p.step}")

    for i in range(args.iterations):
        print(f"\n--- Iteration {i + 1}/{args.iterations} ---")
        spsa_iteration(
            params,
            i,
            args.a,
            args.c,
            args.alpha,
            args.gamma,
            args.engine,
            args.games,
            args.tc,
            args.concurrency,
            args.cutechess,
        )

        for p in params:
            print(f"  {p.name}: {p.value:.2f}")

        result = {p.name: {"value": round(p.value), "default": p.default} for p in params}
        Path(args.output).write_text(json.dumps(result, indent=2))

    print("\nFinal values:")
    for p in params:
        print(f"  {p.name}: {round(p.value)} (default: {p.min_val})")

    result = {p.name: {"value": round(p.value), "default": p.default} for p in params}
    Path(args.output).write_text(json.dumps(result, indent=2))
    print(f"\nResults saved to {args.output}")


if __name__ == "__main__":
    main()
