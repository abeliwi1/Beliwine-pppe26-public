"""
forkjoinsum_startup_cost.py

Separates ForkJoinSum's one-time costs from its steady-state work:

  pool-init   -- new ForkJoinPool(threads): allocates bookkeeping arrays
                 sized by `threads`, does NOT eagerly spawn worker threads.
  first-call  -- the first pool.invoke(): this is where worker threads
                 actually spin up (lazily) plus JIT interpretation before
                 compute() gets JIT-compiled.
  steady-state-- median of later invoke() calls, once threads exist and
                 the hot path is JIT-compiled.

Runs each thread count REPEATS times (fresh JVM each time, since pool-init
and first-call are one-shot events that can't be repeated meaningfully
within a single process) and reports median + spread, then fits a line
against thread count to see whether each cost is ~flat (fixed cost) or
grows with thread count.
"""

import subprocess
import re
import sys
import numpy as np

N = 50_000_000
THREAD_COUNTS = [1, 2, 4, 6, 8, 10]
REPEATS = int(sys.argv[1]) if len(sys.argv) > 1 else 8


def run_once(n_threads):
    result = subprocess.run(
        ["java", "ForkJoinSum.java", str(N), str(n_threads)],
        capture_output=True, text=True
    )
    out = result.stdout
    pool_init = float(re.search(r"pool-init time=([0-9.]+)", out).group(1))
    first_call = float(re.search(r"first-call\s+sum=\d+\s+time=([0-9.]+)", out).group(1))
    steady = [float(x) for x in re.findall(r"forkjoin\s+sum=\d+\s+time=([0-9.]+)", out)]
    return pool_init, first_call, np.median(steady)


print(f"Running {REPEATS} repeats per thread count...\n")
pool_init_ms = {}
first_call_ms = {}
steady_ms = {}

for t in THREAD_COUNTS:
    pi, fc, st = [], [], []
    for _ in range(REPEATS):
        p, f, s = run_once(t)
        pi.append(p * 1000)
        fc.append(f * 1000)
        st.append(s * 1000)
    pool_init_ms[t] = pi
    first_call_ms[t] = fc
    steady_ms[t] = st
    print(f"  {t:2d} threads:  pool-init median={np.median(pi):7.3f}/mean={np.mean(pi):7.3f} ms   "
          f"first-call median={np.median(fc):7.3f}/mean={np.mean(fc):7.3f} ms (±{np.std(fc):.3f})   "
          f"steady median={np.median(st):7.3f}/mean={np.mean(st):7.3f} ms")

threads = np.array(THREAD_COUNTS, dtype=float)
pool_init_med = np.array([np.median(pool_init_ms[t]) for t in THREAD_COUNTS])
first_call_med = np.array([np.median(first_call_ms[t]) for t in THREAD_COUNTS])
steady_med = np.array([np.median(steady_ms[t]) for t in THREAD_COUNTS])
first_call_minus_steady = first_call_med - steady_med  # the "extra" cold cost

def fit_line(x, y):
    slope, intercept = np.polyfit(x, y, 1)
    return slope, intercept

pi_slope, pi_intercept = fit_line(threads, pool_init_med)
fc_slope, fc_intercept = fit_line(threads, first_call_med)
extra_slope, extra_intercept = fit_line(threads, first_call_minus_steady)

print("\n--- Linear fit:  cost(threads) = slope * threads + intercept ---")
print(f"pool-init            slope={pi_slope:8.4f} ms/thread   intercept={pi_intercept:7.3f} ms")
print(f"first-call (total)   slope={fc_slope:8.4f} ms/thread   intercept={fc_intercept:7.3f} ms")
print(f"first-call - steady  slope={extra_slope:8.4f} ms/thread   intercept={extra_intercept:7.3f} ms")

def verdict(slope, intercept, label):
    # a slope small relative to the intercept means "fixed", not "scales with threads"
    if intercept > 1e-6 and abs(slope) / intercept < 0.05:
        print(f"  -> {label}: FIXED cost (slope negligible vs. intercept)")
    else:
        direction = "grows" if slope > 0 else "shrinks"
        print(f"  -> {label}: {direction} with thread count "
              f"(~{slope:.3f} ms per additional thread)")

print()
verdict(pi_slope, pi_intercept, "pool-init")
verdict(fc_slope, fc_intercept, "first-call")
verdict(extra_slope, extra_intercept, "first-call minus steady-state (pure cold-start overhead)")

# --- Plot ---
import matplotlib.pyplot as plt

fig, axes = plt.subplots(1, 2, figsize=(11, 5))
fig.suptitle(f"ForkJoinSum Startup Costs vs. Thread Count  (N={N:,}, {REPEATS} repeats)", fontsize=12)

ax = axes[0]
for t in THREAD_COUNTS:
    ax.scatter([t] * REPEATS, pool_init_ms[t], color="steelblue", alpha=0.4, s=20)
    ax.scatter([t] * REPEATS, first_call_ms[t], color="tomato", alpha=0.4, s=20)
ax.plot(THREAD_COUNTS, pool_init_med, "o-", color="steelblue", label="pool-init (median)")
ax.plot(THREAD_COUNTS, first_call_med, "o-", color="tomato", label="first-call (median)")
ax.plot(THREAD_COUNTS, steady_med, "o-", color="seagreen", label="steady-state (median)")
ax.set_xlabel("Threads")
ax.set_ylabel("Time (ms)")
ax.set_title("Raw costs")
ax.set_xticks(THREAD_COUNTS)
ax.legend()

ax = axes[1]
ax.plot(THREAD_COUNTS, first_call_minus_steady, "o-", color="purple",
         label="first-call − steady-state")
ax.axhline(0, color="gray", linestyle=":")
fit_y = extra_slope * threads + extra_intercept
ax.plot(threads, fit_y, "--", color="gray",
        label=f"fit: {extra_slope:.3f}·threads + {extra_intercept:.3f}")
ax.set_xlabel("Threads")
ax.set_ylabel("Extra cold-start cost (ms)")
ax.set_title("Isolated cold-start overhead")
ax.set_xticks(THREAD_COUNTS)
ax.legend()

plt.tight_layout()
out = "forkjoinsum_startup_cost.png"
plt.savefig(out, dpi=150)
print(f"\nSaved {out}")
