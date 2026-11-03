#!/bin/sh
# Run every version of the shared increment, with the same thread count.
# Usage: ./run.sh [threads]     (default 4)
cd "$(dirname "$0")"
for p in Race Volatile WrongLock Synchronized ExplicitLock SpinLock Atomic Local; do
    java $p.java "${1:-4}"
done
