/*
 * WrongLock.java -- synchronized on the wrong lock
 *
 * Every increment is inside a synchronized method, and the total is still
 * wrong. A synchronized method locks this, and each task has its own
 * Counter, so each task takes a different lock and no task ever waits. But
 * count is static: one copy, shared by every task. The lock's scope must
 * match the scope of the data.
 *
 * The fix is one word: a static synchronized method locks the class, which
 * every task shares.
 *
 * Usage: java WrongLock.java [threads]     (default 4)
 */
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class WrongLock {
    static class Counter {
        static long count = 0;                  // one copy, shared by every task
        synchronized void increment() {         // locks this: one lock per Counter
            count++;
        }
    }

    static final int N = 1_000_000;

    public static void main(String[] args) {
        int nthreads = args.length > 0 ? Integer.parseInt(args[0]) : 4;

        long start = System.nanoTime();
        try (ExecutorService pool = Executors.newFixedThreadPool(nthreads)) {
            for (int t = 0; t < nthreads; t++) {
                pool.submit(() -> {
                    Counter counter = new Counter();    // this task's own lock
                    for (int i = 0; i < N; i++) {
                        counter.increment();
                    }
                });
            }
        }
        long ms = (System.nanoTime() - start) / 1_000_000;

        System.out.printf("WrongLock     expected %,d  got %,d  (%d ms)%n",
                          (long) nthreads * N, Counter.count, ms);
    }
}
