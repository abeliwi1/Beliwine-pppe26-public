/*
 * SpinQueue.java -- a bounded queue that waits by spinning
 *
 * The simplest way to wait for the queue to change: take the lock, look,
 * and if the queue is full (or empty), let go of the lock and try again.
 * It is correct: every test and every update happens while holding the
 * lock. But a waiting thread never sleeps. It keeps a core busy, and keeps
 * taking the lock that the thread it is waiting for needs.
 *
 * Thread.onSpinWait() is the processor's spin-wait hint (x86 PAUSE, Arm
 * YIELD), from Lecture 13.
 *
 * Usage: java SpinQueue.java [pairs] [capacity]   (default 4 pairs, capacity 4)
 */
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicLong;

public class SpinQueue {

    // A queue of longs in a circular array: items[head] is the oldest, and
    // the next put goes count slots after it.
    static class BoundedQueue {
        private final long[] items;
        private int head = 0;
        private int count = 0;

        BoundedQueue(int capacity) { items = new long[capacity]; }

        void put(long x) {
            while (true) {
                synchronized (this) {
                    if (count < items.length) {         // room: put and return
                        items[(head + count) % items.length] = x;
                        count++;
                        return;
                    }
                }                                       // full: let go, try again
                Thread.onSpinWait();
            }
        }

        long take() {
            while (true) {
                synchronized (this) {
                    if (count > 0) {                    // an item: take and return
                        long x = items[head];
                        head = (head + 1) % items.length;
                        count--;
                        return x;
                    }
                }                                       // empty: let go, try again
                Thread.onSpinWait();
            }
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
        System.out.printf("SpinQueue     %d producers, %d consumers, capacity %d: %s  (%d ms)%n",
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
