"""sample.py -- record which CPU every thread of some processes is on, over time.

    python3 sample.py --matmul PID --worker PID [PID ...] [--interval 0.01]
                      [--duration 10] [--out run.csv]

Every interval, for each PID, read /proc/<pid>/task/<tid>/stat for every
thread and write one CSV row per thread: t_seconds, workload, pid, tid, state,
cpu.  "state" is R (running or runnable) or S/D (sleeping); "cpu" is the
processor field, the CPU the thread last ran on.  Only R samples mean the
thread is on that core.
"""
import argparse
import csv
import os
import time


def read_stat(path):
    """Return (state, cpu) from a /proc/.../stat file, or None if it is gone."""
    try:
        with open(path) as f:
            s = f.read()
    except (FileNotFoundError, ProcessLookupError):
        return None
    # Field 2 is "(comm)" and comm may contain spaces or ')', so split after
    # the last ')'.  What follows starts at field 3 (state); processor is 39.
    rest = s[s.rindex(")") + 2:].split()
    state, cpu = rest[0], int(rest[39 - 3])
    try:
        with open(path[:-4] + "schedstat") as f:
            run_ns = int(f.read().split()[0])
    except (FileNotFoundError, ProcessLookupError):
        return None
    return state, cpu, run_ns


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--matmul", type=int, nargs="*", default=[])
    ap.add_argument("--worker", type=int, nargs="*", default=[])
    ap.add_argument("--interval", type=float, default=0.010)
    ap.add_argument("--duration", type=float, default=10.0)
    ap.add_argument("--out", default="samples.csv")
    args = ap.parse_args()

    pids = [("matmul", p) for p in args.matmul] + [("worker", p) for p in args.worker]
    with open(args.out, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["t_seconds", "workload", "pid", "tid", "state", "cpu", "run_ns"])
        t0 = time.monotonic()
        k = 0
        while True:
            t = time.monotonic() - t0
            if t >= args.duration:
                break
            for workload, pid in pids:
                try:
                    tids = os.listdir(f"/proc/{pid}/task")
                except FileNotFoundError:
                    continue
                for tid in tids:
                    r = read_stat(f"/proc/{pid}/task/{tid}/stat")
                    if r:
                        w.writerow([f"{t:.4f}", workload, pid, tid, r[0], r[1], r[2]])
            # Sleep to the next tick on an absolute schedule so we don't drift.
            k += 1
            time.sleep(max(0.0, t0 + k * args.interval - time.monotonic()))


if __name__ == "__main__":
    main()
