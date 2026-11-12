# Ordering: waiting for another thread

A lock keeps two threads out of the same code at once. These examples are
about the other half of synchronization: making a thread wait until another
thread has done something.

Every program runs as a single file, with no build step (JDK 11 or later):

```
java MonitorQueue.java          4 producers, 4 consumers, capacity 4 (the defaults)
java NotifyQueue.java 4 1       4 producers, 4 consumers, capacity 1
./run.sh                        every example once
```

## A bounded blocking queue

Producers put items into a queue of fixed capacity, and consumers take them
out. A `put` must wait while the queue is full, and a `take` while it is empty.

Every queue version runs the same test: P producers and P consumers, 100,000
items per producer, every item different. If each item arrives exactly once,
the consumers' sums add up to 1 + 2 + ... + 100,000·P, and the program prints
`correct`. It prints `WRONG` if the sum is off, `FAILED` if a thread threw an
exception, and `STUCK` if threads are still waiting after 10 seconds.

| program | how a thread waits | result |
|---|---|---|
| [SpinQueue.java](SpinQueue.java) | takes the lock, looks, lets go and tries again | correct, but a waiting thread keeps a core busy |
| [MonitorQueue.java](MonitorQueue.java) | `synchronized`, `while (...) wait()`, `notifyAll()` | correct: Java's built-in condition variable |
| [IfQueue.java](IfQueue.java) | the same, with `if` instead of `while` | **wrong**: a thread woken too late takes from an empty queue |
| [NotifyQueue.java](NotifyQueue.java) | the same, with `notify()` instead of `notifyAll()` | **hangs**: a consumer wakes a consumer, and the producers sleep on |
| [ConditionQueue.java](ConditionQueue.java) | a `ReentrantLock` with two conditions, `notFull` and `notEmpty`, and `signal()` | correct: each wakeup goes to a thread that can use it |
| [LibraryQueue.java](LibraryQueue.java) | `ArrayBlockingQueue` or `LinkedBlockingQueue` | correct: what to use in real code |

On an Apple M5, one run of each:

```
SpinQueue     4 producers, 4 consumers, capacity 4: correct  (235 ms)
MonitorQueue  4 producers, 4 consumers, capacity 4: correct  (935 ms)
IfQueue       4 producers, 4 consumers, capacity 4: FAILED: java.lang.ArrayIndexOutOfBoundsException: Index -2 out of bounds for length 4  (34 ms)
ConditionQueue 4 producers, 4 consumers, capacity 4: correct  (1298 ms)
NotifyQueue   4 producers, 4 consumers, capacity 1: STUCK: threads still waiting after 10 s  (10006 ms)
LibraryQueue  array, 4 producers, 4 consumers, capacity 4: correct  (1214 ms)
LibraryQueue  linked, 4 producers, 4 consumers, capacity 4: correct  (435 ms)
```

`IfQueue` fails differently from run to run: an exception, or a wrong sum.
`NotifyQueue` hung on every run with capacity 1, but usually finishes with
capacity 4: the bug is there either way, and a bigger queue only makes it
rarer. The times are from one run with no warmup, so they are not a benchmark.

## Measuring the cost: QueueBench

[QueueBench.java](QueueBench.java) times every correct queue, with 1 pair of
threads up to one thread per core:

```
java QueueBench.java            capacity 1024: threads rarely wait
java QueueBench.java 1          capacity 1: every item is a handoff
java QueueBench.java 1 3        capacity 1, at most 3 pairs
```

  * the total of 1,000,000 items is split evenly across the producers
  * all threads start together, behind a latch; the clock stops when the
    last consumer finishes
  * each point is the fastest of 5 runs, after a warmup that lets the JIT
    compile every queue
  * the result is nanoseconds per item: wall-clock time / items
  * every run checks the consumers' sum, and stops if it is wrong

Each capacity takes a few minutes; capacity 1 took almost 10 on an M5.
Results: [results.M5.md](results.M5.md).

## Other ways to wait

| program | what it waits for |
|---|---|
| [Barrier.java](Barrier.java) | a `CyclicBarrier`: every thread finishes step s before any starts step s+1. Run with `nobarrier` to see the answer change |
| [Futures.java](Futures.java) | a `Future`: the result of one task. `submit` is Cilk's spawn, `get` its sync. A `CompletableFuture` says what to do when the result arrives |
| [Deadlock.java](Deadlock.java) | two locks taken in opposite orders: each thread waits for the other forever. Run with `ordered` for the fix |

A `CountDownLatch` is the remaining one: wait until something has happened N
times. `SyncBench.java` and `QueueBench.java` use two as a starting gun.
