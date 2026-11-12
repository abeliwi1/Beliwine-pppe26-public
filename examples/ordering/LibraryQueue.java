/*
 * LibraryQueue.java -- the same producers and consumers on java.util.concurrent's queues
 *
 * In real code, don't write a blocking queue: use one.
 *   ArrayBlockingQueue   a circular array with one lock and two conditions,
 *                        like ConditionQueue.java
 *   LinkedBlockingQueue  a linked list with two locks, one for put and one
 *                        for take, so a producer and a consumer don't wait
 *                        for each other unless the queue is empty or full
 *
 * Both hold objects, so each long is boxed into a Long.
 *
 * Usage: java LibraryQueue.java [pairs] [capacity] [array|linked]
 *        (default 4 pairs, capacity 4, array)
 */
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicLong;

public class LibraryQueue {

    // The library's queue, behind the same put and take as the other versions.
    static class BoundedQueue {
        private final BlockingQueue<Long> q;

        BoundedQueue(int capacity) {
            q = kind.equals("linked") ? new LinkedBlockingQueue<>(capacity)
                                      : new ArrayBlockingQueue<>(capacity);
        }

        void put(long x) throws InterruptedException { q.put(x); }
        long take() throws InterruptedException { return q.take(); }
    }

    static String kind = "array";

    static final int ITEMS = 100_000;           // items per producer

    // pairs producers and pairs consumers share one queue. Producer p puts
    // p*ITEMS + 1 ... p*ITEMS + ITEMS, so every item is different; each
    // consumer takes ITEMS items. If every item arrives exactly once, the
    // consumers' sums add up to 1 + 2 + ... + pairs*ITEMS.
    public static void main(String[] args) throws Exception {
        int pairs = args.length > 0 ? Integer.parseInt(args[0]) : 4;
        int capacity = args.length > 1 ? Integer.parseInt(args[1]) : 4;
        if (args.length > 2) kind = args[2];
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
        System.out.printf("LibraryQueue  %s, %d producers, %d consumers, capacity %d: %s  (%d ms)%n",
                          kind, pairs, pairs, capacity, outcome(finished, tasks, total.get(), n * (n + 1) / 2), ms);
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
