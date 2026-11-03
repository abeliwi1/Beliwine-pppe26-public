/*
 * SyncBench.java -- what each way of synchronizing a shared counter costs
 *
 * Every correct method from the examples increments one shared counter,
 * and the benchmark times each one with 2, 3, ... threads, up to one per
 * core. The total number of increments is fixed and split evenly across
 * the threads. The result is wall-clock nanoseconds per increment: the
 * time for the whole run divided by the number of increments.
 *
 * Two benchmarks, picked by the first argument:
 *   contended  every thread increments back to back: the worst case, the
 *              threads fight over the counter all the time
 *   work       each thread does W steps of private arithmetic (~1.5 ns
 *              per step) between increments: the common case, the
 *              threads meet at the counter now and then
 *
 * In contended, a method that updates one shared variable can't get
 * faster with more threads: the updates happen one at a time. The
 * question is how much slower each one gets. LongAdder and Local don't
 * update one variable, so they can. In work, the private work runs in parallel, so the time per
 * increment should fall as threads are added, until the counter becomes
 * the bottleneck.
 *
 * LongAdder is java.util.concurrent's counter for this job: each thread
 * that collides with another adds to a cell of its own, and sum() adds
 * the cells -- Local, done by the library.
 *
 * Usage: java SyncBench.java contended|work [maxThreads] [W]
 *   (default maxThreads = Runtime.availableProcessors(), W = 500)
 */
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicLong;
import java.util.concurrent.atomic.LongAdder;
import java.util.concurrent.locks.ReentrantLock;

public class SyncBench {
    static final long TOTAL = 2_000_000;    // increments per measurement, split across the threads
    static final int REPS = 5;              // timed runs per point; report the fastest

    static final String[] METHODS =
        {"synchronized", "ReentrantLock", "SpinLock", "AtomicLong", "LongAdder", "Local"};

    // the shared state for each method
    static long count = 0;                  // synchronized, ReentrantLock, SpinLock, Local
    static final Object monitor = new Object();
    static final ReentrantLock lock = new ReentrantLock();
    static final AtomicBoolean held = new AtomicBoolean(false);
    static final AtomicLong atomic = new AtomicLong();
    static final LongAdder adder = new LongAdder();

    static int W = 0;                       // steps of private work between increments
    static long sink;                       // keeps the private work from being optimized away

    // private work: a chain of dependent steps, ~1.5 ns each on an M5. The
    // xor-shift matters: with x * a + b alone, the JIT folds pairs of steps
    // into one (x * a*a + (a*b + b)) and the work shrinks tenfold.
    static long work(long x) {
        for (int i = 0; i < W; i++) {
            x = (x ^ (x >>> 7)) * 6364136223846793005L + i;
        }
        return x;
    }

    // one thread's share of the run: ops increments by one method
    static long run(int method, long ops, long x) {
        switch (method) {
            case 0:                                         // synchronized
                for (long i = 0; i < ops; i++) {
                    x = work(x);
                    synchronized (monitor) { count++; }
                }
                break;
            case 1:                                         // ReentrantLock
                for (long i = 0; i < ops; i++) {
                    x = work(x);
                    lock.lock();
                    try { count++; } finally { lock.unlock(); }
                }
                break;
            case 2:                                         // SpinLock
                for (long i = 0; i < ops; i++) {
                    x = work(x);
                    while (!held.compareAndSet(false, true)) Thread.onSpinWait();
                    count++;
                    held.set(false);
                }
                break;
            case 3:                                         // AtomicLong
                for (long i = 0; i < ops; i++) {
                    x = work(x);
                    atomic.incrementAndGet();
                }
                break;
            case 4:                                         // LongAdder
                for (long i = 0; i < ops; i++) {
                    x = work(x);
                    adder.increment();
                }
                break;
            case 5:                                         // Local
                long mine = 0;
                for (long i = 0; i < ops; i++) {
                    x = work(x);
                    mine++;
                }
                synchronized (monitor) { count += mine; }
                break;
        }
        return x;
    }

    static void reset() {
        count = 0;
        atomic.set(0);
        adder.reset();
    }

    static long value(int method) {
        if (method == 3) return atomic.get();
        if (method == 4) return adder.sum();
        return count;
    }

    // time one run: all threads start together, the clock stops when the last one finishes
    static double measure(ExecutorService pool, int nthreads, int method) throws Exception {
        long ops = TOTAL / nthreads;
        reset();

        CountDownLatch ready = new CountDownLatch(nthreads);
        CountDownLatch go = new CountDownLatch(1);
        List<Future<Long>> results = new ArrayList<>();
        for (int t = 0; t < nthreads; t++) {
            long seed = t + 1;
            results.add(pool.submit(() -> {
                ready.countDown();
                go.await();                 // wait for the starting gun
                return run(method, ops, seed);
            }));
        }
        ready.await();                      // every thread is waiting at the gate

        long start = System.nanoTime();
        go.countDown();
        for (Future<Long> r : results) sink += r.get();
        long ns = System.nanoTime() - start;

        if (value(method) != ops * nthreads)
            throw new IllegalStateException(METHODS[method] + ": wrong count " + value(method));
        return (double) ns / (ops * nthreads);
    }

    static double fastest(ExecutorService pool, int nthreads, int method) throws Exception {
        double best = Double.MAX_VALUE;
        for (int r = 0; r < REPS; r++) best = Math.min(best, measure(pool, nthreads, method));
        return best;
    }

    public static void main(String[] args) throws Exception {
        String mode = args.length > 0 ? args[0] : "contended";
        int maxThreads = args.length > 1 ? Integer.parseInt(args[1])
                                         : Runtime.getRuntime().availableProcessors();
        if (mode.equals("work")) W = args.length > 2 ? Integer.parseInt(args[2]) : 500;
        else if (!mode.equals("contended")) throw new IllegalArgumentException("mode: contended or work");

        // warm up: let the JIT compile every method, and the cores reach full clock
        try (ExecutorService pool = Executors.newFixedThreadPool(maxThreads)) {
            for (int round = 0; round < 3; round++)
                for (int m = 0; m < METHODS.length; m++) measure(pool, maxThreads, m);
        }

        // the private work by itself, on one thread
        long start = System.nanoTime();
        long x = 1;
        for (long i = 0; i < TOTAL; i++) x = work(x);
        sink += x;
        double workNs = (double) (System.nanoTime() - start) / TOTAL;

        System.out.printf("# %s: %,d increments per point, fastest of %d; ns per increment%n",
                          mode, TOTAL, REPS);
        System.out.printf("# %s %s, %d cores, Java %s%n", System.getProperty("os.name"),
                          System.getProperty("os.arch"),
                          Runtime.getRuntime().availableProcessors(),
                          System.getProperty("java.version"));
        System.out.printf("# private work between increments: W = %d, %.1f ns on one thread%n",
                          W, workNs);
        System.out.printf("%7s", "threads");
        for (String m : METHODS) System.out.printf("%15s", m);
        System.out.println();

        for (int n = 2; n <= maxThreads; n++) {
            System.out.printf("%7d", n);
            try (ExecutorService pool = Executors.newFixedThreadPool(n)) {
                for (int m = 0; m < METHODS.length; m++) System.out.printf("%15.1f", fastest(pool, n, m));
            }
            System.out.println();
        }
        if (sink == 42) System.out.println();   // use sink, so the JIT must compute it
    }
}
