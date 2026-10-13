#!/bin/sh
# run.sh -- run the multithreaded and multiprocess workloads side by side and
# record where every thread runs.  Writes fits.csv and oversubscribed.csv.
set -e
cd "$(dirname "$0")"
make -s

# One CPU per physical core: 0-3 are the Zen 5 cores, 4-11 the Zen 5c cores.
# See README.threads_on_cores.md for how this was read from /sys.
CPUS=0-11
SECONDS_RUN=10

run() {   # run NAME THREADS PROCESSES
    name=$1; k=$2; n=$3
    # Warm the clock: spin every core for a second so none starts from idle.
    taskset -c $CPUS ./fork_workers 12 1 > /dev/null

    OMP_NUM_THREADS=$k taskset -c $CPUS ./omp_matmul $((SECONDS_RUN + 1)) > matmul.pid &
    taskset -c $CPUS ./fork_workers $n $((SECONDS_RUN + 1)) > workers.out &
    sleep 0.2
    matmul=$(head -1 matmul.pid)
    workers=$(head -n $n workers.out | tr '\n' ' ')
    echo "$name: matmul pid $matmul ($k threads), worker pids $workers"
    python3 sample.py --matmul $matmul --worker $workers \
        --duration $SECONDS_RUN --out $name.csv
    wait
    rm -f matmul.pid workers.out
}

run fits 4 4
run oversubscribed 8 8
