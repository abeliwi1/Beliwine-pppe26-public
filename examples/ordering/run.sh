#!/bin/sh
# Run every ordering example once.
# Usage: ./run.sh [pairs] [capacity]     (default 4 pairs, capacity 4)
cd "$(dirname "$0")"
for p in SpinQueue MonitorQueue IfQueue ConditionQueue; do
    java $p.java "${1:-4}" "${2:-4}"
done
java NotifyQueue.java "${1:-4}" 1          # hangs most reliably with capacity 1
java LibraryQueue.java "${1:-4}" "${2:-4}" array
java LibraryQueue.java "${1:-4}" "${2:-4}" linked
java Barrier.java
java Barrier.java 4 nobarrier
java Futures.java
java Deadlock.java
java Deadlock.java ordered
