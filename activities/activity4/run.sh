#!/usr/bin/env bash
# run.sh -- every version on every input, three runs, on the 4 Zen 5 cores
# (CPUs 0-3) of a Ryzen AI 9 HX 370.  Rebuilds simpson from your
# simpson.linux.cpp first.  Writes results.txt; plot it with
# python3 plot_results.py.
set -euo pipefail
cd "$(dirname "$0")"
~/opencilk/bin/clang++ -fopencilk -fopenmp -O3 -std=c++17 simpson.linux.cpp -o simpson
: > results.txt
for r in 1 2 3; do
  for in in poly trig exp; do
    for v in omp_static omp_dynamic omp_task; do
      OMP_PLACES=cores OMP_PROC_BIND=spread OMP_NUM_THREADS=4 taskset -c 0-3 ./simpson $v $in | tee -a results.txt
    done
    for v in cilk_for cilk_for_g1 cilk_spawn; do
      CILK_NWORKERS=4 taskset -c 0-3 ./simpson $v $in | tee -a results.txt
    done
  done
done
