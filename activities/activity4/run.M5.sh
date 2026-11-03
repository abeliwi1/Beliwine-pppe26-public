#!/usr/bin/env bash
# run.M5.sh -- every version on every input, three runs, on the 4 performance
# cores of an Apple M5.  Writes results.M5.txt; plot it with
# python3 plot_results.py results.M5.txt speedup.M5.png "4 M5 performance cores".
set -euo pipefail
cd "$(dirname "$0")"
~/opencilk/bin/clang++ -fopencilk -O3 -std=c++17 \
  -Xpreprocessor -fopenmp -I/opt/homebrew/opt/libomp/include \
  -L/opt/homebrew/opt/libomp/lib -lomp simpson.M5.cpp -o simpson.M5
: > results.M5.txt
for r in 1 2 3; do
  for in in poly trig exp; do
    for v in omp_static omp_dynamic omp_task; do
      OMP_NUM_THREADS=4 ./simpson.M5 $v $in | tee -a results.M5.txt
    done
    for v in cilk_for cilk_for_g1 cilk_spawn; do
      CILK_NWORKERS=4 ./simpson.M5 $v $in | tee -a results.M5.txt
    done
  done
done
