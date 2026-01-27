#!/usr/bin/env python3
"""Minimal i8085 simulator harness.

This is a placeholder runner intended for future simulator integration.
Set I8085_SIM to the simulator executable, and optionally I8085_SIM_ARGS
for extra arguments. The script runs the simulator with the provided
binary and checks the exit code / stdout when requested.
"""

import argparse
import os
import shlex
import subprocess
import sys


def main() -> int:
    parser = argparse.ArgumentParser(description="Run an i8085 simulator")
    parser.add_argument("binary", help="path to binary/ELF to run")
    parser.add_argument("--sim", help="simulator executable (default: $I8085_SIM)")
    parser.add_argument(
        "--sim-arg",
        dest="sim_args",
        action="append",
        default=[],
        help="extra simulator argument (repeatable)",
    )
    parser.add_argument("--expect-exit", type=int, default=0)
    parser.add_argument("--expect-stdout")
    parser.add_argument("--expect-stdout-file")
    args = parser.parse_args()

    sim = args.sim or os.environ.get("I8085_SIM")
    if not sim:
        print("error: I8085_SIM not set and --sim not provided", file=sys.stderr)
        return 2

    sim_args = []
    env_args = os.environ.get("I8085_SIM_ARGS")
    if env_args:
        sim_args.extend(shlex.split(env_args))
    sim_args.extend(args.sim_args)

    cmd = [sim] + sim_args + [args.binary]
    result = subprocess.run(cmd, text=True, capture_output=True)

    if result.returncode != args.expect_exit:
        print("error: simulator exit code mismatch", file=sys.stderr)
        print(f"  expected: {args.expect_exit}", file=sys.stderr)
        print(f"  actual:   {result.returncode}", file=sys.stderr)
        if result.stdout:
            print("stdout:", file=sys.stderr)
            print(result.stdout, file=sys.stderr)
        if result.stderr:
            print("stderr:", file=sys.stderr)
            print(result.stderr, file=sys.stderr)
        return 1

    expected_stdout = None
    if args.expect_stdout_file:
        with open(args.expect_stdout_file, "r", encoding="utf-8") as f:
            expected_stdout = f.read()
    elif args.expect_stdout is not None:
        expected_stdout = args.expect_stdout

    if expected_stdout is not None:
        if result.stdout != expected_stdout:
            print("error: simulator stdout mismatch", file=sys.stderr)
            print("expected:", file=sys.stderr)
            print(expected_stdout, file=sys.stderr)
            print("actual:", file=sys.stderr)
            print(result.stdout, file=sys.stderr)
            return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
