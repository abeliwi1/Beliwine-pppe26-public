#!/bin/sh
# run.sh -- true sharing (ping-pong) and false sharing for the four pairs of
# CPUs on a Ryzen AI 9 HX 370 (CPUs 0-3 Zen 5, 4-11 Zen 5c, 12-23 the second
# hardware thread of each core).
set -e
cd "$(dirname "$0")"
~/opencilk/bin/clang++ -fopencilk -O3 -std=c++17 sharing.cpp -o sharing
for mode in pause spin false padded false-fence padded-fence false-atomic padded-atomic; do
  CILK_NWORKERS=2 ./sharing 0 12 $mode    # same core
  CILK_NWORKERS=2 ./sharing 0 1  $mode    # two Zen 5 cores
  CILK_NWORKERS=2 ./sharing 4 5  $mode    # two Zen 5c cores
  CILK_NWORKERS=2 ./sharing 0 4  $mode    # one Zen 5 and one Zen 5c
done
