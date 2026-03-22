#!/usr/bin/env python3

import argparse
import subprocess
import sys


OPENING_FENS = [
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
    "r1bqkbnr/pppp1ppp/2n5/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 2 3",
    "r1bqkbnr/pppp1ppp/2n5/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R b KQkq - 3 3",
    "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
]


def run_probe(
    path: str, fen: str, depth: int, time_ms: int, threads: int
) -> dict[str, str]:
    proc = subprocess.run(
        [
            path,
            f"--fen={fen}",
            f"--depth={depth}",
            f"--time_ms={time_ms}",
            f"--threads={threads}",
        ],
        capture_output=True,
        text=True,
        timeout=max(30, time_ms // 10),
    )

    data: dict[str, str] = {
        "returncode": str(proc.returncode),
        "stdout": proc.stdout,
        "stderr": proc.stderr,
    }
    for line in proc.stdout.splitlines():
        if "=" not in line:
            continue
        key, value = line.split("=", 1)
        data[key.strip()] = value.strip()
    return data


def adjudicate_terminal(score_text: str) -> str:
    score = int(score_text)
    if score > 0:
        return "white"
    if score < 0:
        return "black"
    return "draw"


def play_game(
    game_no: int,
    opening_fen: str,
    white_name: str,
    white_probe: str,
    black_name: str,
    black_probe: str,
    depth: int,
    time_ms: int,
    threads: int,
    max_plies: int,
) -> tuple[str, str, int]:
    fen = opening_fen
    plies = 0

    while plies < max_plies:
        side_to_move = fen.split()[1]
        engine_name = white_name if side_to_move == "w" else black_name
        engine_probe = white_probe if side_to_move == "w" else black_probe
        opponent_name = black_name if side_to_move == "w" else white_name

        result = run_probe(engine_probe, fen, depth, time_ms, threads)
        if result["returncode"] != "0":
            return (
                opponent_name,
                f"{engine_name} probe failure rc={result['returncode']}",
                plies,
            )

        if result.get("legal") != "true":
            return (
                opponent_name,
                f"{engine_name} illegal move {result.get('bestmove', 'unknown')}",
                plies,
            )

        if result.get("terminal") == "true":
            winner = adjudicate_terminal(result.get("score", "0"))
            if winner == "draw":
                return "draw", f"terminal score {result.get('score', '0')}", plies
            return (
                white_name if winner == "white" else black_name,
                (f"terminal score {result.get('score', '0')}"),
                plies,
            )

        next_fen = result.get("next_fen", "")
        if not next_fen or next_fen == "INVALID":
            return opponent_name, f"{engine_name} invalid next_fen", plies

        fen = next_fen
        plies += 1

    return "draw", f"ply cap {max_plies}", plies


def main() -> int:
    parser = argparse.ArgumentParser(description="Run a direct search-probe match")
    parser.add_argument("--white-probe", required=True)
    parser.add_argument("--black-probe", required=True)
    parser.add_argument("--white-name", default="white")
    parser.add_argument("--black-name", default="black")
    parser.add_argument("--depth", type=int, default=64)
    parser.add_argument("--time-ms", type=int, default=80)
    parser.add_argument("--threads", type=int, default=1)
    parser.add_argument("--max-plies", type=int, default=80)
    parser.add_argument("--games", type=int, default=8)
    args = parser.parse_args()

    results = {args.white_name: 0.0, args.black_name: 0.0, "draw": 0.0}
    total_games = 0

    pairings = [
        (args.white_name, args.white_probe, args.black_name, args.black_probe),
        (args.black_name, args.black_probe, args.white_name, args.white_probe),
    ]

    for opening_fen in OPENING_FENS:
        for white_name, white_probe, black_name, black_probe in pairings:
            if total_games >= args.games:
                break
            total_games += 1
            winner, reason, plies = play_game(
                total_games,
                opening_fen,
                white_name,
                white_probe,
                black_name,
                black_probe,
                args.depth,
                args.time_ms,
                args.threads,
                args.max_plies,
            )
            if winner == "draw":
                results[args.white_name] += 0.5
                results[args.black_name] += 0.5
                results["draw"] += 1.0
            else:
                results[winner] += 1.0
            print(
                f"game={total_games} white={white_name} black={black_name} "
                f"winner={winner} reason={reason} plies={plies}"
            )
        if total_games >= args.games:
            break

    print(
        f"FINAL {args.white_name}={results[args.white_name]:.1f} "
        f"{args.black_name}={results[args.black_name]:.1f} draws={results['draw']:.0f} "
        f"games={total_games}"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
