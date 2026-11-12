/*
 * QueueBench.java -- what each correct blocking queue costs
 *
 * pairs producers put a fixed total of items through one queue, and pairs
 * consumers take them out. The result is wall-clock nanoseconds per item:
 * the time for the whole run divided by the number of items. Every point
 * is the fastest of 5 runs, after a warmup that lets the JIT compile every
 * queue, and every run checks that each item arrived exactly once.
 *
 * The queues are the correct ones from the examples:
 *   spin       SpinQueue: test under the lock, let go and retry
 *   monitor    MonitorQueue: synchronized, wait() and notifyAll()
 *   condition  ConditionQueue: a ReentrantLock with notFull and notEmpty
 *   array      java.util.concurrent.ArrayBlockingQueue
 *   linked     java.util.concurrent.LinkedBlockingQueue
 *
 * The capacity decides how often a thread has to wait. With capacity 1,
 * every item is a handoff: the producer can't put the next one until a
 * consumer has taken this one. With a large capacity, producers and
 * consumers rarely find the queue full or empty, and the cost is mostly
 * the lock.
 *
 * Usage: java QueueBench.java [capacity] [maxPairs]
 *   (default capacity 1024, maxPairs = Runtime.availableProcessors() / 2)
 */
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.locks.Condition;
import java.util.concurrent.locks.ReentrantLock;

public class QueueBench {
    static final long TOTAL = 1_000_000;    // items per measurement, split across the producers
    static final int REPS = 5;              // timed runs per point; report the fastest

    static final String[] QUEUES = {"spin", "monitor", "condition", "array", "linked"};

    interface Queue {
        void put(long x) throws InterruptedException;
        long take() throws InterruptedException;
    }

    static class SpinQueue implements Queue {
        private final long[] items;
        private int head = 0, count = 0;
        SpinQueue(int capacity) { items = new long[capacity]; }

        public void put(long x) {
            while (true) {
                synchronized (this) {
                    if (count < items.length) {
                        items[(head + count) % items.length] = x;
                        count++;
                        return;
                    }
                }
                Thread.onSpinWait();
            }
        }

        public long take() {
            while (true) {
                synchronized (this) {
                    if (count > 0) {
                        long x = items[head];
                        head = (head + 1) % items.length;
                        count--;
                        return x;
                    }
                }
                Thread.onSpinWait();
            }
        }
    }

    static class MonitorQueue implements Queue {
        private final long[] items;
        private int head = 0, count = 0;
        MonitorQueue(int capacity) { items = new long[capacity]; }

        public synchronized void put(long x) throws InterruptedException {
            while (count == items.length) wait();
            items[(head + count) % items.length] = x;
            count++;
            notifyAll();
        }

        public synchronized long take() throws InterruptedException {
            while (count == 0) wait();
            long x = items[head];
            head = (head + 1) % items.length;
            count--;
            notifyAll();
            return x;
        }
    }

    static class ConditionQueue implements Queue {
        private final long[] items;
        private int head = 0, count = 0;
        private final ReentrantLock lock = new ReentrantLock();
        private final Condition notFull = lock.newCondition();
        private final Condition notEmpty = lock.newCondition();
        ConditionQueue(int capacity) { items = new long[capacity]; }

        public void put(long x) throws InterruptedException {
            lock.lock();
            try {
                while (count == items.length) notFull.await();
                items[(head + count) % items.length] = x;
                count++;
                notEmpty.signal();
            } finally {
                lock.unlock();
            }
        }

        public long take() throws InterruptedException {
            lock.lock();
            try {
                while (count == 0) notEmpty.await();
                long x = items[head];
                head = (head + 1) % items.length;
                count--;
                notFull.signal();
                return x;
            } finally {
                lock.unlock();
            }
        }
    }

    static class Library implements Queue {
        private final BlockingQueue<Long> q;
        Library(BlockingQueue<Long> q) { this.q = q; }
        public void put(long x) throws InterruptedException { q.put(x); }
        public long take() throws InterruptedException { return q.take(); }
    }

    static Queue make(int which, int capacity) {
        switch (which) {
            case 0: return new SpinQueue(capacity);
            case 1: return new MonitorQueue(capacity);
            case 2: return new ConditionQueue(capacity);
            case 3: return new Library(new ArrayBlockingQueue<>(capacity));
            default: return new Library(new LinkedBlockingQueue<>(capacity));
        }
    }

    // time one run: all threads start together, the clock stops when the last one finishes
    static double measure(ExecutorService pool, int pairs, int which, int capacity) throws Exception {
        long per = TOTAL / pairs;
        Queue q = make(which, capacity);

        CountDownLatch ready = new CountDownLatch(2 * pairs);
        CountDownLatch go = new CountDownLatch(1);
        List<Future<Long>> sums = new ArrayList<>();
        for (int p = 0; p < pairs; p++) {
            long base = p * per;
            pool.submit(() -> {                                 // a producer
                ready.countDown();
                go.await();
                for (long i = 1; i <= per; i++) q.put(base + i);
                return null;
            });
            sums.add(pool.submit(() -> {                        // a consumer
                ready.countDown();
                go.await();
                long sum = 0;
                for (long i = 0; i < per; i++) sum += q.take();
                return sum;
            }));
        }
        ready.await();                      // every thread is waiting at the gate

        long start = System.nanoTime();
        go.countDown();
        long total = 0;
        for (Future<Long> s : sums) total += s.get();
        long ns = System.nanoTime() - start;

        long n = per * pairs;
        if (total != n * (n + 1) / 2)
            throw new IllegalStateException(QUEUES[which] + ": wrong sum " + total);
        return (double) ns / n;
    }

    static double fastest(ExecutorService pool, int pairs, int which, int capacity) throws Exception {
        double best = Double.MAX_VALUE;
        for (int r = 0; r < REPS; r++) best = Math.min(best, measure(pool, pairs, which, capacity));
        return best;
    }

    public static void main(String[] args) throws Exception {
        int capacity = args.length > 0 ? Integer.parseInt(args[0]) : 1024;
        int maxPairs = args.length > 1 ? Integer.parseInt(args[1])
                                       : Math.max(1, Runtime.getRuntime().availableProcessors() / 2);

        // warm up: let the JIT compile every queue, and the cores reach full clock
        try (ExecutorService pool = Executors.newFixedThreadPool(2 * maxPairs)) {
            for (int round = 0; round < 3; round++)
                for (int w = 0; w < QUEUES.length; w++) measure(pool, maxPairs, w, capacity);
        }

        System.out.printf("# capacity %d: %,d items per point, fastest of %d; ns per item%n",
                          capacity, TOTAL, REPS);
        System.out.printf("# %s %s, %d cores, Java %s%n", System.getProperty("os.name"),
                          System.getProperty("os.arch"),
                          Runtime.getRuntime().availableProcessors(),
                          System.getProperty("java.version"));
        System.out.printf("%7s", "pairs");
        for (String q : QUEUES) System.out.printf("%11s", q);
        System.out.println();

        for (int pairs = 1; pairs <= maxPairs; pairs++) {
            System.out.printf("%7d", pairs);
            try (ExecutorService pool = Executors.newFixedThreadPool(2 * pairs)) {
                for (int w = 0; w < QUEUES.length; w++)
                    System.out.printf("%11.1f", fastest(pool, pairs, w, capacity));
            }
            System.out.println();
        }
    }
}
