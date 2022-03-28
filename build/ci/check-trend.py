#!/usr/bin/env python3
import os
import sys
import argparse
import json
from typing import IO


def check_trend(history_trend_path: IO):
    history = json.load(history_trend_path)
    if len(history) < 2:
        print("At least two test runs are required to see any trends.")
        return

    current = history[0]["data"]
    previous = history[1]["data"]

    print("Current test results, compared to the previous test run:\n")

    fmt = lambda x: f"{x:+5d}".replace("+0", " 0")
    print(f"Failed tests:  {current['failed']:5d} ({fmt(current['failed']-previous['failed'])})")
    print(f"Broken tests:  {current['broken']:5d} ({fmt(current['broken']-previous['broken'])})")
    print(f"Skipped tests: {current['skipped']:5d} ({fmt(current['skipped']-previous['skipped'])})")
    print(f"Passed tests:  {current['passed']:5d} ({fmt(current['passed']-previous['passed'])})")
    print()

    failures_diff = (current["failed"] + current["broken"]) - (previous["failed"] + previous["broken"])
    if failures_diff > 0:
        if "GITHUB_ACTION" in os.environ:
            print("::error::", end="")
        print(f"This commit contains {failures_diff} regression(s).")
        sys.exit(1)
    elif failures_diff == 0:
        if "GITHUB_ACTION" in os.environ:
            print("::notice::", end="")
        print("This commit contains no regressions.")
    else:
        if "GITHUB_ACTION" in os.environ:
            print("::notice::", end="")
        print(f"This commit resolves {-failures_diff} test failure(s).")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--history-trend", type=argparse.FileType("r"), required=True)
    args = parser.parse_args()

    check_trend(args.history_trend)


if __name__ == "__main__":
    main()
