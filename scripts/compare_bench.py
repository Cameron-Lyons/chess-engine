#!/usr/bin/env python3

import json
import sys


def load(path):
    with open(path, "r", encoding="utf-8") as handle:
        return json.load(handle)


def numeric_fields(row):
    return {key: value for key, value in row.items() if isinstance(value, (int, float))}


def result_rows(payload):
    rows = payload.get("results")
    if isinstance(rows, list):
        return rows
    single = payload.get("result")
    if isinstance(single, dict):
        row = dict(single)
        row.setdefault("id", "result")
        return [row]
    return []


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: compare_bench.py old.json new.json", file=sys.stderr)
        return 2

    old = load(sys.argv[1])
    new = load(sys.argv[2])

    if old.get("benchmark") != new.get("benchmark"):
        print("benchmark names do not match", file=sys.stderr)
        return 2

    benchmark = old.get("benchmark", "unknown")
    print(f"Benchmark: {benchmark}")

    old_rows = {row["id"]: row for row in result_rows(old) if "id" in row}
    new_rows = {row["id"]: row for row in result_rows(new) if "id" in row}
    row_ids = sorted(set(old_rows) & set(new_rows))

    for row_id in row_ids:
        print(f"\n[{row_id}]")
        old_numeric = numeric_fields(old_rows[row_id])
        new_numeric = numeric_fields(new_rows[row_id])
        for key in sorted(set(old_numeric) & set(new_numeric)):
            old_value = old_numeric[key]
            new_value = new_numeric[key]
            delta = new_value - old_value
            pct = 0.0 if old_value == 0 else (delta / old_value) * 100.0
            print(
                f"  {key}: {old_value} -> {new_value}  delta={delta:+.3f} ({pct:+.2f}%)"
            )

    old_summary = old.get("summary", {})
    new_summary = new.get("summary", {})
    if old_summary and new_summary:
        print("\n[summary]")
        for key in sorted(set(old_summary) & set(new_summary)):
            old_value = old_summary[key]
            new_value = new_summary[key]
            if not isinstance(old_value, (int, float)) or not isinstance(
                new_value, (int, float)
            ):
                continue
            delta = new_value - old_value
            pct = 0.0 if old_value == 0 else (delta / old_value) * 100.0
            print(
                f"  {key}: {old_value} -> {new_value}  delta={delta:+.3f} ({pct:+.2f}%)"
            )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
