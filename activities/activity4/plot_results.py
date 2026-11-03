import re
import sys
import statistics
import math
from collections import defaultdict
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

# python3 plot_results.py [results.txt] [speedup.png] ["4 Zen 5 cores"]
src = sys.argv[1] if len(sys.argv) > 1 else "results.txt"
dst = sys.argv[2] if len(sys.argv) > 2 else "speedup.png"
cores = sys.argv[3] if len(sys.argv) > 3 else "4 Zen 5 cores"

d = defaultdict(list)
for l in open(src):
    m = re.match(r"(\S+)\s+(\S+)\s+eps.*speedup\s+([\d.]+)x", l)
    if m:
        d[(m.group(2), m.group(1))].append(float(m.group(3)))

inputs = [("trig", "trig\nwork spread evenly"),
          ("poly", "poly\nheavy at both ends"),
          ("exp", "exp (chirp)\nheavy near x = 1")]
versions = [("omp_static", "OpenMP static"), ("omp_dynamic", "OpenMP dynamic"),
            ("omp_task", "OpenMP tasks"), ("cilk_for", "cilk_for (default grain)"),
            ("cilk_for_g1", "cilk_for grainsize(1)"), ("cilk_spawn", "cilk_spawn")]
errors = []
for key, _ in inputs:
    for version, _ in versions:
        values = d[(key, version)]
        if len(values) != 3 or any(not math.isfinite(v) or v <= 0 for v in values):
            errors.append(f"{version}/{key}: expected 3 positive finite speedups, found {len(values)}")
if errors:
    raise SystemExit("Incomplete or invalid benchmark results:\n" + "\n".join(errors))

colors = ["#2a78d6", "#eb6834", "#1baf7a", "#eda100", "#e87ba4", "#008300"]
INK, MUTED, GRID, SURF = "#0b0b0b", "#52514e", "#e4e3df", "#fcfcfb"

fig, ax = plt.subplots(figsize=(11, 5.6), facecolor=SURF)
ax.set_facecolor(SURF)
bw, gap = 0.13, 0.02
for gi, (key, _) in enumerate(inputs):
    for vi, (v, label) in enumerate(versions):
        s = d[(key, v)]
        hi, lo, med = max(s), min(s), statistics.median(s)
        x = gi + (vi - 2.5) * (bw + gap)
        ax.bar(x, med, width=bw, color=colors[vi], label=label if gi == 0 else None,
               zorder=3, edgecolor=SURF, linewidth=1)
        if hi - lo > 0.02:
            ax.plot([x, x], [lo, hi], color=INK, linewidth=1.2, zorder=4)
        ax.text(x, hi + 0.1, f"{med:.1f}", ha="center", va="bottom", fontsize=8, color=MUTED, zorder=5,
                bbox=dict(boxstyle="square,pad=0.1", facecolor=SURF, edgecolor="none"))

ax.hlines(4, -0.55, 2.55, color=MUTED, linestyle="--", linewidth=1, zorder=2)
ax.text(2.6, 4.0, "perfect\n(4 cores)", ha="left", va="center", fontsize=9, color=MUTED)
ax.set_xticks(range(len(inputs)))
ax.set_xticklabels([t for _, t in inputs], fontsize=10, color=INK)
ax.set_ylabel("speedup over serial", color=INK)
ax.set_ylim(0, max(4.6, max(max(v) for v in d.values()) + 0.6))
ax.set_xlim(-0.55, 2.95)
ax.yaxis.grid(True, color=GRID, zorder=0)
for side in ("top", "right"):
    ax.spines[side].set_visible(False)
for side in ("left", "bottom"):
    ax.spines[side].set_color(GRID)
ax.tick_params(colors=MUTED)
ax.set_title(f"Adaptive Simpson on {cores}: speedup by data set and scheduler\n"
             "bar = median of 3 runs, black line = min–max", fontsize=12, color=INK, loc="left")
ax.legend(loc="upper center", bbox_to_anchor=(0.5, -0.17), ncol=3, frameon=False, fontsize=9)
fig.tight_layout()
fig.savefig(dst, dpi=130, facecolor=SURF)
print("wrote", dst)
