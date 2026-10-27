#!/bin/sh
# run.M5.sh -- true sharing (ping-pong) and false sharing for the three pairs
# of cores on an Apple M5: two performance cores, two efficiency cores, and
# one of each.  macOS can't pin threads, so sharing.M5 places them by QoS.
set -e
cd "$(dirname "$0")"
~/opencilk/bin/clang++ -fopencilk -O3 -std=c++17 sharing.M5.cpp -o sharing.M5
for mode in pause spin false padded false-fence padded-fence false-atomic padded-atomic; do
  CILK_NWORKERS=2 ./sharing.M5 P P $mode    # two performance cores
  CILK_NWORKERS=2 ./sharing.M5 E E $mode    # two efficiency cores
  CILK_NWORKERS=2 ./sharing.M5 P E $mode    # one of each
done
