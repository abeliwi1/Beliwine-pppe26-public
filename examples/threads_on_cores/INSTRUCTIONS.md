# Instructions: threads and processes on cores

These instructions are for a Claude Code session on a Linux machine
(AMD Ryzen AI 9 HX 370, Pop!_OS 24.04). They describe an experiment and a figure
for Lecture 09 (`course_materials/09.threads.md`). Do the whole thing without
waiting for further input, then report back as described in the last section.

## The point the figure makes

The OS scheduler places **threads** on cores. A multithreaded program and a set
of single-threaded processes look the same to it: so many runnable threads,
competing for so many cores. The only difference between them is memory: the
threads share one address space, and the processes each have their own.

Run a multithreaded workload and a multiprocess workload side by side, record
which core every thread is on over time, and draw it.

## The machine, and one constraint

This machine has 12 physical cores, 4 Zen 5 (fast) and 8 Zen 5c (slower), and
SMT, so Linux shows 24 logical CPUs. **The lecture does not cover SMT**, so the
figure must not show it:

1. Find the topology from `/sys/devices/system/cpu/cpu*/topology/`
   (`core_id`, `thread_siblings_list`) and the core types from
   `/sys/devices/system/cpu/cpu*/cpufreq/cpuinfo_max_freq` (the Zen 5 cores have
   the higher maximum). Do not assume CPU numbering; read it.
2. Choose **one logical CPU per physical core**: 12 CPUs. Run both workloads
   under `taskset -c <those 12>`, so only those CPUs are used.
3. Label figure rows by physical core: "Zen 5 core 0" to "Zen 5 core 3", then
   "Zen 5c core 4" to "Zen 5c core 11". Never "CPU 17" or "logical CPU".

Record the mapping you used in the README (see below).

## What to write

Put everything in `examples/threads_on_cores/`. C for the workloads, Python for
the sampler and the plot (the repo's `requirements.txt` has matplotlib).

### `fork_workers.c`: the multiprocess workload

Mirror the `fork` example in Lecture 09, so students recognise it.

* `./fork_workers N SECONDS`: the parent `fork()`s N children and `wait()`s for
  all of them.
* Each child runs one steady, CPU-bound, single-threaded loop for SECONDS and
  exits. Use a Monte Carlo estimate of pi with a small inline xorshift
  generator (no `rand()`, no locks, no memory traffic to speak of). Check the
  clock only every ~1M iterations.
* Name each child with `prctl(PR_SET_NAME, "worker")` so it is easy to find,
  and have the parent print the children's PIDs.
* Keep it short and readable: it is meant to be shown in class. About 40 lines.

### `omp_matmul.c`: the multithreaded workload

* `OMP_NUM_THREADS=K ./omp_matmul SECONDS`: repeat a single-precision matrix
  multiply (n = 1024 is fine) with `#pragma omp parallel for` over rows of C,
  until SECONDS have passed. Use `i,k,j` loop order so it is not
  pathologically slow. Name the process `matmul`.
* The threads must stay busy for the whole run: put the repeat loop *outside*
  the parallel region, or use one parallel region with an `omp for` inside a
  timed loop. Check that `top -H` shows K threads at ~100% each.
* Optional alternative, behind a command-line flag or a second binary: the same
  run using OpenBLAS `cblas_sgemm` with `OPENBLAS_NUM_THREADS=K`. Only add it if
  OpenBLAS is already installed (`libopenblas-dev`); do not install packages.

Build both with the system `gcc -O2` (`-fopenmp` for the matmul). Add a short
`Makefile`.

### `sample.py`: record where every thread runs

* Arguments: the PIDs to watch (the matmul's PID and each worker's PID), a
  sampling interval (default 10 ms) and a duration.
* Every interval, for each PID, read every `/proc/<pid>/task/<tid>/stat`. Take
  field 3 (state) and field 39 (`processor`, the CPU it last ran on). Parse
  carefully: field 2 is the command name in parentheses and may contain spaces,
  so split after the last `)`.
* Write a CSV: `t_seconds, workload, pid, tid, state, cpu`, where workload is
  `matmul` or `worker`.
* Treat a sample as "running on that core" only if the state is `R`.
* Also record the run with `perf sched record` if `perf` is available and
  usable without root, and use `perf sched timehist` to cross-check the
  sampled placement and to count migrations. If perf is not usable, skip it and
  say so.

### `run.sh`: the experiments

Warm the clock first (the activity harnesses spin on a high-IPC loop; do the
same for ~1 s). Then, for each run, start both workloads within a fraction of a
second of each other under the same `taskset`, start the sampler, and let it
run 10 seconds. Two runs:

| run | matmul threads | worker processes | total | what it should show |
|---|---|---|---|---|
| `fits` | 4 | 4 | 8, fewer than 12 cores | where the scheduler puts each kind of thread, and whether the fast cores go to one workload |
| `oversubscribed` | 8 | 8 | 16, more than 12 cores | cores shared in turns, and threads moving between cores |

Save each CSV as `fits.csv` and `oversubscribed.csv`.

### `plot_timeline.py`: the figure

One figure per run, saved as SVG to
`course_materials/images/threads_on_cores_fits.svg` and
`course_materials/images/threads_on_cores_oversubscribed.svg`.

* Rows: the 12 physical cores, Zen 5 at the top, a gap, then Zen 5c, with a
  label for each group. Time across, in seconds.
* For each sample where a thread is in state `R`, draw a short bar in that
  core's row. Colour by workload, using the lecture's figure colours:
  * matmul threads in blues (base `#2b6cb0`), one shade per thread
  * worker processes in oranges (base `#c05621`), one shade per process
* A legend that says "matmul: 1 process, K threads" and "workers: K processes,
  1 thread each".
* Title, axis labels, white background, readable at about 1000 px wide.
* Under or beside the figure, a small table: for each thread, the share of its
  running time on Zen 5 vs Zen 5c, and how many times it changed core.

### `README.threads_on_cores.md`

Short, in the style of the other example READMEs: what it shows, how to build
and run it, the CPU mapping used, and the measured results (the share-of-time
table and migration counts for both runs). State what you observed, not what
you expected. If the scheduler gave the fast cores to one workload, or
didn't, say which.

## Rules

* Measure; don't assume. If a result is surprising, re-run to check it is
  stable before writing it down.
* Don't edit `course_materials/09.threads.md` or any lecture. Only add the files
  above and the two images.
* Don't mention SMT, hyperthreading or logical CPUs in the figure or README
  text students will read. The README may record the CPU numbers used, as a
  "machine setup" note.
* Don't install packages or use sudo.

## Report back

When done, commit everything in `examples/threads_on_cores/` and the two SVGs
on a new branch `threads-on-cores` and push it. Then reply with:

1. the two figures' paths,
2. the share-of-time table and migration counts for both runs,
3. one paragraph on what the figures show,
4. anything that didn't work as described here.
