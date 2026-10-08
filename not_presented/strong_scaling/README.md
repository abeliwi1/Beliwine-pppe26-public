# Strong Scaling and Amdahl's Law

A revision of `02_strong_scaling.ipynb` from
[pcds.2024](https://github.com/randalburns/pcds.2024/blob/main/ebook/lectures.2023/02_strong_scaling.ipynb),
rebuilt around a measurement taken on this machine rather than illustrative numbers.

* `02_strong_scaling.ipynb` — the lecture
* `scaling_bench.c` — the benchmark it plots
* `ccx1.csv` — 8 identical Zen 5c cores in one L3 domain, three serial fractions x 15 reps
* `scaling.csv` — the same program on cpus 0..23 (mixed core types, then SMT)
* `schedule.csv` — `static` vs `guided` at p=0.95, 1..12 threads
* `percore.csv`, `zones.csv`, `zone_sizes.csv`, `locality.csv` — the diagnostic experiments

```
gcc -O2 -fopenmp -o scaling_bench scaling_bench.c -lm
./scaling_bench --cpus 4,5,6,7,8,9,10,11 --serial-steps 322000,680000,2040000 \
                --max-threads 8 --reps 15 > ccx1.csv
./scaling_bench --serial-steps 322000,680000,2040000 --max-threads 24 --reps 15 > scaling.csv
```

## What changed from the original

**Corrections**

* the text said "parallel fraction (.95)" over code that used `p=0.92`
* the commented-out "embedded for loops" alternative indexed a `(len(resources), len(p))`
  array as `[i,j]` with `i` over `p` — it raises `IndexError` if uncommented
* the author's note "I tried to fool around with map/vectorize here and failed" is resolved:
  the whole table is one broadcast expression
* `p/\infty` replaced with an actual limit
* `<img src="../images/al2.png">` pointed outside the notebook's directory; figures are now
  generated in-notebook

**Logical flaws**

* *ideal speedup is an upper bound on all speedup* — false. Superlinear speedup from cache
  aggregation is real; the activity asked students to draw the bound as if it were universal
* $s$ (speedup of the improved part) and $n$ (processor count) were used interchangeably
  without ever stating the substitution $s = n$, which is the model's strongest assumption
* the Karp-Flatt values were computed and then averaged into one number, discarding the
  diagnostic — a *trend* in $e$ is the entire point of computing it across $n$
* $T(1)$ was never pinned down as the best serial algorithm, so relative and absolute speedup
  are conflated

**Added**

* parallel efficiency $E = S/n$, which the original referred to without defining
* a measured curve on eight *identical* cores, where the model works: monotonic, and a
  near-constant 84% of the predicted speedup, with Karp-Flatt's $e$ flattening out. The lecture
  now shows the model succeeding before it shows it failing
* then the same program on mixed cores, where the curve is non-monotonic — a shape Amdahl's law
  cannot produce — and $e$ jumps 4.4x with a discontinuity at the thread that changes core type
* the cause, established by a three-experiment chain rather than asserted. Two hypotheses fit
  the dip and are confounded, because on this part CCX0 *is* the four Zen 5 cores: heterogeneous
  core speed, or crossing the L3 domain boundary. Per-core timing shows the cores really are
  1.5x apart; holding the worker set fixed and moving only the data's home domain shows locality
  costs nothing here. It is core heterogeneity meeting `schedule(static)`. Switching to `guided`
  removes the dip and improves 12-thread speedup by over 60%
* a `taskset` pitfall that inverted the first answer: a process can call `sched_setaffinity` and
  widen the mask `taskset` gave it, so every "per-core" probe had migrated back to cpu 0 and
  reported all twelve cores as identical
* an overhead-aware model $T(n) = T_s + T_p/n + cn$, fitted to the data — where it **fails**,
  with the failure explained rather than hidden
* a section on measurement hygiene, from three defects found while producing the data: libgomp's
  spinning idle threads (turned a 5x speedup into a 0.5x slowdown), cold-clock penalty on the
  first process to run, and ascending-order sweeps putting the T(1) baseline in the least
  representative part of the run
* a table of what the law assumes against what the machine actually did

## Scope

Measured on the Ryzen only. The heterogeneous-core discussion in section 3 applies to parts
built that way; the rest is general.
