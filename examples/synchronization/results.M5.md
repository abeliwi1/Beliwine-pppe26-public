# What synchronization costs: Apple M5 results

[SyncBench.java](SyncBench.java) on an Apple M5 (4 performance cores and 6
efficiency cores), Java 26.0.2.1, measured 2026-10-28 on one run of each
benchmark. Every number is nanoseconds per increment: wall-clock time
divided by the 2,000,000 increments, the fastest of 5 runs.

```
java SyncBench.java contended
java SyncBench.java work
```

macOS has no way to pin a thread to a core, so the OS decides where each
thread runs. With 4 threads or fewer, they can all be on performance cores;
from 5 on, some must run on efficiency cores.

## contended: back-to-back increments

| threads | synchronized | ReentrantLock | SpinLock | AtomicLong | LongAdder | Local |
|---:|---:|---:|---:|---:|---:|---:|
| 2 | 40.1 | 56.9 | 66.8 | 3.9 | 2.2 | 0.6 |
| 3 | 114.2 | 6.6 | 42.3 | 6.3 | 1.5 | 0.4 |
| 4 | 132.0 | 10.5 | 55.4 | 8.2 | 1.1 | 0.4 |
| 5 | 96.5 | 5.4 | 119.7 | 10.4 | 0.9 | 0.4 |
| 6 | 134.4 | 5.1 | 145.3 | 11.9 | 0.8 | 0.3 |
| 7 | 151.5 | 5.4 | 165.0 | 12.6 | 0.7 | 0.3 |
| 8 | 144.9 | 5.1 | 163.4 | 14.3 | 0.6 | 0.3 |
| 9 | 155.1 | 5.1 | 212.4 | 29.5 | 0.5 | 0.2 |
| 10 | 155.9 | 5.6 | 237.0 | 30.1 | 0.5 | 0.2 |

* **AtomicLong** gets slower as threads are added: 3.9 ns with 2 threads,
  30 ns with 10. Every increment moves the counter's cache line to the core
  that makes it (Lecture 13), and more threads means more moves. The jump at
  9 threads is when most of the threads are on efficiency cores, and the
  line crosses between the two clusters.
* **SpinLock** is the worst at 5 or more threads, 237 ns at 10: every
  waiting thread's `compareAndSet` pulls the lock's line away from the
  thread that holds it.
* **synchronized** costs 100–155 ns from 3 threads on: waiting threads spin
  and then sleep, and handing the lock to a sleeping thread goes through the
  OS.
* **ReentrantLock** is the surprise, at about 5 ns from 3 threads on: faster
  than AtomicLong. The reason is that a thread keeps re-taking the lock while
  most of the others sleep; see [Does one thread hog the
  lock?](#does-one-thread-hog-the-lock) below.
* **LongAdder** and **Local** get *faster* with more threads, because they
  don't update one variable: each thread counts in a place of its own.

## work: ~760 ns of private work between increments

| threads | synchronized | ReentrantLock | SpinLock | AtomicLong | LongAdder | Local |
|---:|---:|---:|---:|---:|---:|---:|
| 2 | 383.3 | 383.3 | 383.2 | 383.2 | 383.2 | 382.8 |
| 3 | 265.2 | 265.3 | 265.2 | 265.3 | 265.2 | 264.9 |
| 4 | 199.1 | 199.2 | 199.2 | 199.1 | 199.5 | 198.8 |
| 5 | 237.9 | 250.4 | 197.8 | 168.4 | 165.2 | 164.6 |
| 6 | 251.1 | 259.5 | 195.0 | 145.6 | 142.2 | 140.9 |
| 7 | 291.9 | 330.6 | 244.1 | 161.5 | 148.6 | 142.9 |
| 8 | 288.5 | 342.7 | 286.1 | 149.5 | 132.7 | 126.9 |
| 9 | 300.1 | 339.1 | 333.3 | 126.9 | 120.5 | 114.6 |
| 10 | 320.9 | 262.5 | 320.0 | 97.9 | 92.2 | 91.8 |

* Up to 4 threads every method is the same, and the time follows the
  work: 757 ns of work on one thread, so 383 ns with 2 threads, 265 with 3,
  199 with 4. The threads rarely meet at the counter, so how it is
  synchronized doesn't matter.
* From 5 threads on, the three locks get *slower* as threads are added:
  320 ns with 10 threads for synchronized, worse than with 4. The lock is
  now busy most of the time, and each handoff to a waiting thread costs far
  more than the increment it protects.
* AtomicLong, LongAdder and Local keep scaling, to about 92–98 ns with 10
  threads. That's short of 757 / 10 = 76 ns because 6 of the 10 cores are
  efficiency cores.

## Does one thread hog the lock?

[LockHog.java](LockHog.java) measures this directly, with bookkeeping inside
the critical section. `fair` is `new ReentrantLock(true)`, which hands the lock
to the thread that has waited longest. Each row is the median of 5 runs;
*busy* is CPU time across all threads divided by wall-clock time, so a sleeping
thread doesn't count and a spinning one does. Measured 2026-11-03.

```
java LockHog.java 10 0        back-to-back increments
java LockHog.java 10 500      ~760 ns of private work between increments
```

Back-to-back increments, 10 threads, two runs of the program:

| method | ns/inc | mean run (increments in a row by one thread) | busy |
|---|---:|---:|---:|
| ReentrantLock | 6.5, 5.4 | 566, 345 | 1.7, 1.9 |
| synchronized | 65.6, 131.9 | 3.6, 1.4 | 2.0, 2.0 |
| SpinLock | 267.7, 256.1 | 2.5, 2.7 | 8.7, 8.4 |
| fair | 2581, 2472 | 1.0, 1.0 | 1.0, 1.0 |

* **The lock does stay with one thread:** 230–570 increments in a row from 4
  to 10 threads, against 1–8 for `synchronized` and the spin lock.
* **Most of the other threads are asleep:** of 10 threads, about 2 are using a
  core at any moment.
* **Forcing fair handoffs removes the advantage:** a run of exactly 1, at about
  2,500 ns per increment, 400× slower.
* **It's short runs, not a monopoly.** The longest run was 3–16% of one
  thread's share, so the lock does move around. And about 2 threads are busy,
  not 1; we haven't determined what the second one is doing.
* **The arithmetic fits, roughly.** If each handoff costs about what a fair one
  does, 2,500 ns spread over 230–570 increments is 4–11 ns per increment, close
  to the 6 ns measured.
* **Unfair doesn't imply long runs.** `synchronized` and the spin lock make no
  fairness promise either, but their runs are short. Why ReentrantLock's are so
  much longer, we haven't determined.
* **With 2 threads there is no hogging,** which accounts for the 56.9 ns in the
  contended table: both threads stay awake (busy 1.9–2.0), runs are 5–9, and it
  costs 39–69 ns. This number also moves the most from run to run.

With private work between increments, a thread that releases the lock is off
doing its work and can't take the lock straight back. The long runs disappear:
ReentrantLock's mean run is 1.0–1.1. At 10 threads its busy count falls to 3.2,
against 5.0 for `synchronized` and 8.3 for the spin lock, so most of its threads
are asleep rather than working. That fits ReentrantLock being the slowest of the
three locks in this test from 6 threads on, and in SyncBench's `work` from 5 to 9
threads, but not its being the fastest of the three at 10 threads in SyncBench
(262.5 ns). We haven't shown that sleeping is the cause.

The bookkeeping changes what it measures. In `work` it adds writes to the data
the lock protects, and the times at 4 threads are 243–308 ns against 199 ns in
the table above (this table is the median, that one the fastest). Use LockHog
for run lengths and busy counts, and SyncBench for the costs.

## What to take from it

* with little contention, every method costs the same: pick the simplest
* under contention, the ranking is Local and LongAdder, then AtomicLong,
  then the locks, with one exception
* the exception: ReentrantLock is fast under contention because the lock
  stays with one thread for hundreds of increments while most of the others
  sleep, which is a poor model of any program that does work between updates
