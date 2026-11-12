/*
 * MonitorQueue.java -- a bounded blocking queue with Java's built-in monitor
 *
 * Producers put items into a queue of fixed capacity, and consumers take
 * them out. A put must wait while the queue is full, and a take while it is
 * empty. Mutual exclusion isn't enough for that: a thread has to wait until
 * another thread has changed the queue.
 *
 * wait() and notifyAll() do it. Both are called while holding the object's
 * mutex (inside a synchronized method):
 *   wait()       releases the mutex and sleeps, as one step, until another
 *                thread calls notifyAll() on the same object; it takes the
 *                mutex back before it returns
 *   notifyAll()  wakes every thread waiting on the object
 *
 * The condition is tested in a while loop, never an if: when a thread
 * wakes up, another thread may already have taken the item it was woken
 * for (IfQueue.java), and Java allows a wait() to return for no reason at
 * all (a spurious wakeup).
 *
 * Usage: java MonitorQueue.java [pairs] [capacity]   (default 4 pairs, capacity 4)
 */
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicLong;

public class MonitorQueue {

    // A queue of longs in a circular array: items[head] is the oldest, and
    // the next put goes count slots after it.
    static class BoundedQueue {
        private final long[] items;
        private int head = 0;
        private int count = 0;

        BoundedQueue(int capacity) { items = new long[capacity]; }

        synchronized void put(long x) throws InterruptedException {
            while (count == items.length)       // full: wait for a take
                wait();
            items[(head + count) % items.length] = x;
            count++;
            notifyAll();                        // wake any thread waiting to take
        }

        synchronized long take() throws InterruptedException {
            while (count == 0)                  // empty: wait for a put
                wait();
            long x = items[head];
            head = (head + 1) % items.length;
            count--;
            notifyAll();                        // wake any thread waiting to put
            return x;
        }
    }

    static final int ITEMS = 100_000;           // items per producer

    // pairs producers and pairs consumers share one queue. Producer p puts
    // p*ITEMS + 1 ... p*ITEMS + ITEMS, so every item is different; each
    // consumer takes ITEMS items. If every item arrives exactly once, the
    // consumers' sums add up to 1 + 2 + ... + pairs*ITEMS.
    public static void main(String[] args) throws Exception {
        int pairs = args.length > 0 ? Integer.parseInt(args[0]) : 4;
        int capacity = args.length > 1 ? Integer.parseInt(args[1]) : 4;
        BoundedQueue q = new BoundedQueue(capacity);
        AtomicLong total = new AtomicLong();

        long start = System.nanoTime();
        ExecutorService pool = Executors.newFixedThreadPool(2 * pairs);
        List<Future<?>> tasks = new ArrayList<>();
        for (int p = 0; p < pairs; p++) {
            long base = (long) p * ITEMS;
            tasks.add(pool.submit(() -> {                       // a producer
                for (int i = 1; i <= ITEMS; i++) q.put(base + i);
                return null;
            }));
            tasks.add(pool.submit(() -> {                       // a consumer
                long sum = 0;
                for (int i = 0; i < ITEMS; i++) sum += q.take();
                total.addAndGet(sum);
                return null;
            }));
        }
        pool.shutdown();
        boolean finished = pool.awaitTermination(10, TimeUnit.SECONDS);
        long ms = (System.nanoTime() - start) / 1_000_000;

        long n = (long) pairs * ITEMS;
        System.out.printf("MonitorQueue  %d producers, %d consumers, capacity %d: %s  (%d ms)%n",
                          pairs, pairs, capacity, outcome(finished, tasks, total.get(), n * (n + 1) / 2), ms);
        System.exit(0);                         // stops any threads that are stuck waiting
    }

    static String outcome(boolean finished, List<Future<?>> tasks, long got, long expected)
            throws InterruptedException {
        for (Future<?> f : tasks) {
            if (!f.isDone()) continue;
            try { f.get(); }
            catch (ExecutionException e) { return "FAILED: " + e.getCause(); }
        }
        if (!finished) return "STUCK: threads still waiting after 10 s";
        return got == expected ? "correct" : String.format("WRONG: sum %,d, expected %,d", got, expected);
    }
}
