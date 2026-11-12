# What a blocking queue costs: Apple M5 results

[QueueBench.java](QueueBench.java) on an Apple M5 (4 performance cores and 6
efficiency cores), Java 26.0.2.1, on mains power, measured 2026-10-31 on one
run of each capacity. Every number is nanoseconds per item: wall-clock time
divided by the 1,000,000 items, the fastest of 5 runs. `pairs` is the number
of producers, and also the number of consumers, so 5 pairs is 10 threads, one
per core.

```
java QueueBench.java          capacity 1024
java QueueBench.java 1        capacity 1
```

## Capacity 1024: threads rarely wait

| pairs | spin | monitor | condition | array | linked |
|---:|---:|---:|---:|---:|---:|
| 1 | 95.2 | 130.1 | 49.1 | 61.5 | 117.7 |
| 2 | 303.2 | 141.0 | 19.1 | 22.4 | 56.6 |
| 3 | 299.1 | 143.9 | 23.5 | 25.6 | 36.2 |
| 4 | 328.2 | 154.8 | 36.6 | 29.7 | 36.9 |
| 5 | 299.7 | 148.1 | 29.1 | 30.6 | 39.8 |

## Capacity 1: every item is a handoff

| pairs | spin | monitor | condition | array | linked |
|---:|---:|---:|---:|---:|---:|
| 1 | 361.8 | 3,997.8 | 4,393.7 | 4,541.1 | 4,817.9 |
| 2 | 382.3 | 4,841.5 | 3,928.0 | 4,428.6 | 4,390.3 |
| 3 | 373.9 | 5,541.9 | 3,845.0 | 4,406.9 | 4,555.6 |
| 4 | 379.1 | 6,914.1 | 4,440.7 | 4,755.7 | 4,805.9 |
| 5 | 377.0 | 6,952.7 | 4,591.5 | 4,771.2 | 4,770.0 |

The capacity-1 run took almost 10 minutes, and the JVM spent twice as much
time in the operating system as in Java code (423 s system, 207 s user): the
cost is in putting threads to sleep and waking them.

## What the numbers say

* **With capacity 1, sleeping costs about 4 µs per item.** Every item needs
  a consumer to wake: the producer puts, the consumer is asleep, and waking it
  goes through the operating system. All four queues that sleep cost 4–5 µs
  per item, ten times as much as the spin queue.
* **Spinning wins when there are cores to spare.** With capacity 1 the spin
  queue costs about 370 ns per item at every thread count: a waiting thread
  never sleeps, so a handoff is a cache-line transfer, not an OS wakeup. Here
  there were never more threads than cores (10 at most); with more threads
  than cores, a spinning thread would take a core from the thread it waits
  for. This benchmark doesn't test that.
* **Spinning loses when there is little to wait for.** With capacity 1024,
  threads rarely find the queue full or empty, and the spin queue is the
  slowest, about 300 ns per item from 2 pairs on: every thread is taking and
  releasing the queue's monitor in a tight loop, and the monitor is contended.
* **`notifyAll` costs more as threads are added.** With capacity 1, the
  monitor queue goes from 4.0 µs with 1 pair to 7.0 µs with 5: each item wakes
  every waiting thread, and all but one go back to sleep. The queues that wake
  one thread with `signal()` stay at 4–4.6 µs.
* **With a big queue, the cost is the lock.** At capacity 1024, the monitor
  queue (`synchronized`) costs 130–155 ns per item, and the queues built on a
  `ReentrantLock` (condition, array) 19–37 ns: the same gap as between
  `synchronized` and `ReentrantLock` in Lecture 14's SyncBench.
* **Two locks didn't help here.** `LinkedBlockingQueue` lets a put and a take
  run at the same time, but at capacity 1024 it is slower than
  `ArrayBlockingQueue` (36–40 ns against 22–31 ns from 3 pairs on, and twice
  as slow with 1 pair). It allocates a list node for every item.

## Not yet explained

* At capacity 1024, the condition and array queues are two to three times
  faster with 2 pairs than with 1 (19 against 49 ns, 22 against 62 ns). One
  guess: with one producer and one consumer, the queue swings between full and
  empty, and a thread sleeps at each swing. This hasn't been checked.
