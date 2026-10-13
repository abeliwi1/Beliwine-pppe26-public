"""plot_timeline.py -- draw which core every thread ran on, over time.

    python3 plot_timeline.py fits.csv oversubscribed.csv

For each CSV from sample.py, writes
course_materials/images/threads_on_cores_<name>.svg: one row per physical
core, time across, a bar wherever a thread ran, blue for the matmul's threads
and orange for the worker processes.  A table underneath gives each thread's
time on a core, its split between Zen 5 and Zen 5c, and how often it moved.
"""
import csv
import os
import sys
from collections import defaultdict

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.collections import PolyCollection
from matplotlib.colors import to_rgb
from matplotlib.patches import Patch

ZEN5 = [0, 1, 2, 3]                  # CPU numbers, one per physical core:
ZEN5C = [4, 5, 6, 7, 8, 9, 10, 11]   # see README.threads_on_cores.md
BASE = {"matmul": "#2b6cb0", "worker": "#c05621"}
OUT_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                       "..", "..", "course_materials", "images")


def shades(base, n):
    """n shades of base, from lighter to darker."""
    r, g, b = to_rgb(base)
    out = []
    for i in range(n):
        x = -0.45 + 0.75 * i / max(1, n - 1)   # <0 mixes white, >0 black
        if x < 0:
            out.append(tuple(c + (1 - c) * -x for c in (r, g, b)))
        else:
            out.append(tuple(c * (1 - x) for c in (r, g, b)))
    return out


def load(path):
    """{(workload, tid): [(t, state, cpu, run_ns), ...]} in time order, and
    the set of PIDs.  A process's main thread is the one whose tid is its PID."""
    rows, pids = defaultdict(list), set()
    with open(path) as f:
        for r in csv.DictReader(f):
            pids.add(int(r["pid"]))
            rows[(r["workload"], int(r["tid"]))].append(
                (float(r["t_seconds"]), r["state"], int(r["cpu"]), int(r["run_ns"])))
    return rows, pids


def intervals(samples):
    """(t_start, t_end, cpu, ns_run) for each interval in which the thread ran:
    it was in state R at the end of it and its time on a CPU grew."""
    return [(a[0], b[0], b[2], b[3] - a[3])
            for a, b in zip(samples, samples[1:])
            if b[1] == "R" and b[3] > a[3]]


def plot(path):
    name = os.path.splitext(os.path.basename(path))[0]
    rows, pids = load(path)
    threads = sorted(rows, key=lambda k: (k[0] != "matmul", k[1]))
    n = {w: sum(1 for k in threads if k[0] == w) for w in BASE}
    colour, label = {}, {}
    for w in BASE:
        ks = [k for k in threads if k[0] == w]
        for i, (k, c) in enumerate(zip(ks, shades(BASE[w], len(ks)))):
            colour[k] = c
            label[k] = (f"thread {i}" + (" (main)" if k[1] in pids else "")
                        if w == "matmul" else f"process {i}")

    # Row position of each core: Zen 5 on top, a gap, then Zen 5c.
    ypos = {cpu: 13 - i for i, cpu in enumerate(ZEN5)}
    ypos.update({cpu: 8 - i for i, cpu in enumerate(ZEN5C)})

    # Who ran on each core in each interval.  If two threads took turns on a
    # core within one interval, split the bar's height between them.
    cells = defaultdict(list)
    for k in threads:
        for t0, t1, cpu, ns in intervals(rows[k]):
            if cpu in ypos:
                cells[(t0, t1, cpu)].append((k, ns))

    fig = plt.figure(figsize=(10, 6.2 + 0.22 * len(threads)), facecolor="white")
    ax = fig.add_axes([0.14, 1 - 5.2 / fig.get_figheight(), 0.84,
                       4.4 / fig.get_figheight()])
    # Consecutive intervals with one thread alone on a core become one bar.
    bars = []                                         # (t0, t1, cpu, [(k, ns)])
    for (t0, t1, cpu), who in sorted(cells.items(), key=lambda c: (c[0][2], c[0][0])):
        last = bars[-1] if bars else None
        if (last and len(who) == 1 and len(last[3]) == 1 and last[2] == cpu
                and last[1] == t0 and last[3][0][0] == who[0][0]):
            bars[-1] = (last[0], t1, cpu, last[3])
        else:
            bars.append((t0, t1, cpu, who))
    boxes = defaultdict(list)                         # one collection per thread
    for t0, t1, cpu, who in bars:
        total = sum(ns for _, ns in who)
        y = ypos[cpu] - 0.4
        for k, ns in who:
            h = 0.8 * ns / total
            boxes[k].append([(t0, y), (t1, y), (t1, y + h), (t0, y + h)])
            y += h
    for k, polys in boxes.items():
        ax.add_collection(PolyCollection(polys, facecolors=[colour[k]], linewidths=0,
                                         rasterized=True))   # keeps the SVG small

    ax.set_xlim(0, max(s[-1][0] for s in rows.values()))
    ax.set_ylim(0, 14)
    ax.set_yticks([ypos[c] for c in ZEN5 + ZEN5C])
    ax.set_yticklabels([f"Zen 5 core {i}" for i in range(4)] +
                       [f"Zen 5c core {i}" for i in range(4, 12)])
    ax.axhline(9, color="0.6", lw=0.8)
    ax.text(1.0, 13.55, "Zen 5 cores", transform=ax.get_yaxis_transform(),
            ha="right", va="bottom", fontsize=9, color="0.35")
    ax.text(1.0, 8.55, "smaller Zen 5c cores, lower clock", transform=ax.get_yaxis_transform(),
            ha="right", va="bottom", fontsize=9, color="0.35")
    ax.set_xlabel("time (s)")
    ax.tick_params(axis="y", length=0)
    for s in ("top", "right", "left"):
        ax.spines[s].set_visible(False)
    ax.set_title(f"{n['matmul'] + n['worker']} threads on 12 cores: "
                 f"one {n['matmul']}-thread process and {n['worker']} "
                 "single-threaded processes", fontsize=12)
    ax.legend(handles=[
        Patch(color=BASE["matmul"],
              label=f"matmul: 1 process, {n['matmul']} threads"),
        Patch(color=BASE["worker"],
              label=f"workers: {n['worker']} processes, 1 thread each")],
        loc="upper center", bbox_to_anchor=(0.5, -0.1), ncol=2, frameon=False)

    # Table: time on a core, Zen 5 vs Zen 5c share of it, core changes.
    cell_text, cell_colours = [], []
    for k in threads:
        s = rows[k]
        iv = intervals(s)
        ran = sum(ns for *_, ns in iv)
        fast = sum(ns for _, _, cpu, ns in iv if cpu in ZEN5)
        moves = sum(1 for a, b in zip(s, s[1:]) if a[2] != b[2])
        on = (s[-1][3] - s[0][3]) / 1e9 / (s[-1][0] - s[0][0])
        cell_text.append([f"{k[0]} {label[k]}", f"{on:.0%}", f"{fast / ran:.0%}",
                          f"{1 - fast / ran:.0%}", str(moves)])
        cell_colours.append([colour[k] + (0.35,)] + ["white"] * 4)
    tax = fig.add_axes([0.14, 0.02, 0.84, 0.22 * (len(threads) + 1) / fig.get_figheight()])
    tax.axis("off")
    table = tax.table(cellText=cell_text, cellColours=cell_colours, loc="center",
                      colLabels=["thread", "time on a core", "of which Zen 5",
                                 "of which Zen 5c", "core changes"],
                      colWidths=[0.26, 0.18, 0.18, 0.18, 0.18], cellLoc="center")
    table.auto_set_font_size(False)
    table.set_fontsize(9)
    for c in table.get_celld().values():
        c.set_edgecolor("0.8")

    out = os.path.join(OUT_DIR, f"threads_on_cores_{name}.svg")
    fig.savefig(out, facecolor="white", dpi=150)
    if os.environ.get("PREVIEW"):
        fig.savefig(os.path.join(os.environ["PREVIEW"], name + ".png"), dpi=100)
    print(out)


if __name__ == "__main__":
    for p in sys.argv[1:]:
        plot(p)
