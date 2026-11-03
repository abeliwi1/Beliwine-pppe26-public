/*
 * LockHog.java -- does one thread hog the lock?
 *
 * SyncBench shows ReentrantLock beating AtomicLong under contention. The
 * hypothesis: the lock isn't fair, so a thread that releases it takes it
 * straight back, and does long runs of increments while the others sleep.
 * This program tests that, by measuring for each lock:
 *
 *   ns/inc    wall-clock time per increment, as in SyncBench
 *   mean run  increments in a row by one thread, before another gets the
 *             lock. Measured inside the critical section, so it is exact:
 *             the lock itself protects the bookkeeping.
 *   max run   the longest such run, also as a share of one thread's work:
 *             100% means a thread did its entire share without interruption
 *   busy      CPU time of all the threads / wall-clock time: how many
 *             threads are really using a core. A thread that spins counts
 *             as busy; a thread that sleeps doesn't.
 *
 * "fair" is new ReentrantLock(true), which hands the lock to the thread
 * that has waited longest. It does fewer increments per run, because every
 * handoff can go through the OS.
 *
 * Each row is the median of 5 runs, by ns/inc.
 *
 * Usage: java LockHog.java [maxThreads] [W]
 *   (default maxThreads = Runtime.availableProcessors(), W = 0; W is the
 *    steps of private work between increments, as in SyncBench)
 */
import java.lang.management.ManagementFactory;
import java.lang.management.ThreadMXBean;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Comparator;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.locks.ReentrantLock;

public class LockHog {
    static final long TOTAL = 2_000_000;    // increments per run
    static final long TOTAL_FAIR = 50_000;  // the fair lock is far slower
    static final int REPS = 5;

    static final String[] METHODS = {"synchronized", "ReentrantLock", "fair", "SpinLock"};

    static final Object monitor = new Object();
    static final ReentrantLock unfair = new ReentrantLock();
    static final ReentrantLock fair = new ReentrantLock(true);
    static final AtomicBoolean held = new AtomicBoolean(false);

    // shared state, all protected by the lock under test
    static long count;
    static int owner;                       // the thread that made the last increment
    static long segments;                   // runs so far
    static long runStart;                   // the value of count when this run began
    static long maxRun;

    static int W = 0;
    static long sink;
    static final ThreadMXBean cpu = ManagementFactory.getThreadMXBean();

    static long work(long x) {
        for (int i = 0; i < W; i++) {
            x = (x ^ (x >>> 7)) * 6364136223846793005L + i;
        }
        return x;
    }

    // the critical section: the increment, and a note whenever the lock changed hands
    static void increment(int id) {
        if (owner != id) {
            maxRun = Math.max(maxRun, count - runStart);
            runStart = count;
            owner = id;
            segments++;
        }
        count++;
    }

    static long run(int method, long ops, int id, long x) {
        switch (method) {
            case 0:
                for (long i = 0; i < ops; i++) {
                    x = work(x);
                    synchronized (monitor) { increment(id); }
                }
                break;
            case 1:
            case 2:
                ReentrantLock lock = method == 1 ? unfair : fair;
                for (long i = 0; i < ops; i++) {
                    x = work(x);
                    lock.lock();
                    try { increment(id); } finally { lock.unlock(); }
                }
                break;
            case 3:
                for (long i = 0; i < ops; i++) {
                    x = work(x);
                    while (!held.compareAndSet(false, true)) Thread.onSpinWait();
                    increment(id);
                    held.set(false);
                }
                break;
        }
        return x;
    }

    // one run: {ns/inc, mean run, max run, busy threads}
    static double[] measure(ExecutorService pool, int nthreads, int method) throws Exception {
        long ops = (method == 2 ? TOTAL_FAIR : TOTAL) / nthreads;
        count = 0; owner = -1; segments = 0; runStart = 0; maxRun = 0;

        CountDownLatch ready = new CountDownLatch(nthreads);
        CountDownLatch go = new CountDownLatch(1);
        List<Future<Long>> cpuTimes = new ArrayList<>();
        for (int t = 0; t < nthreads; t++) {
            int id = t;
            cpuTimes.add(pool.submit(() -> {
                ready.countDown();
                go.await();
                long c0 = cpu.getCurrentThreadCpuTime();
                sink += run(method, ops, id, id + 1);
                return cpu.getCurrentThreadCpuTime() - c0;
            }));
        }
        ready.await();

        long start = System.nanoTime();
        go.countDown();
        long cpuNs = 0;
        for (Future<Long> c : cpuTimes) cpuNs += c.get();
        long ns = System.nanoTime() - start;

        if (count != ops * nthreads)
            throw new IllegalStateException(METHODS[method] + ": wrong count " + count);
        maxRun = Math.max(maxRun, count - runStart);    // the final run
        return new double[] {(double) ns / count, (double) count / segments, maxRun,
                             (double) cpuNs / ns};
    }

    public static void main(String[] args) throws Exception {
        int maxThreads = args.length > 0 ? Integer.parseInt(args[0])
                                         : Runtime.getRuntime().availableProcessors();
        if (args.length > 1) W = Integer.parseInt(args[1]);

        // warm up: let the JIT compile every method
        try (ExecutorService pool = Executors.newFixedThreadPool(maxThreads)) {
            for (int round = 0; round < 3; round++)
                for (int m = 0; m < METHODS.length; m++) measure(pool, maxThreads, m);
        }

        System.out.printf("# %s %s, %d cores, Java %s; W = %d; median of %d runs%n",
                          System.getProperty("os.name"), System.getProperty("os.arch"),
                          Runtime.getRuntime().availableProcessors(),
                          System.getProperty("java.version"), W, REPS);
        System.out.printf("%7s %14s %10s %12s %12s %10s %6s%n", "threads", "method", "ns/inc",
                          "mean run", "max run", "of share", "busy");

        for (int n = 2; n <= maxThreads; n += 2) {
            try (ExecutorService pool = Executors.newFixedThreadPool(n)) {
                for (int m = 0; m < METHODS.length; m++) {
                    double[][] runs = new double[REPS][];
                    for (int r = 0; r < REPS; r++) runs[r] = measure(pool, n, m);
                    Arrays.sort(runs, Comparator.comparingDouble(run -> run[0]));
                    double[] mid = runs[REPS / 2];
                    double share = (m == 2 ? TOTAL_FAIR : TOTAL) / n;
                    System.out.printf("%7d %14s %10.1f %12.1f %12.0f %9.1f%% %6.1f%n", n,
                                      METHODS[m], mid[0], mid[1], mid[2],
                                      100 * mid[2] / share, mid[3]);
                }
            }
        }
        if (sink == 42) System.out.println();
    }
}
