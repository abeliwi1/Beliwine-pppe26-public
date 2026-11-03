/*
 * Atomic.java -- the increment as one hardware instruction
 *
 * AtomicLong.incrementAndGet() does the load, add and store as a single
 * atomic instruction (lock xadd on x86, ldadd on ARM). No other thread
 * can come between the three steps, so no lock is needed. This works
 * only when the critical section is one operation the hardware supports:
 * add, exchange, compare-and-set.
 *
 * Usage: java Atomic.java [threads]     (default 4)
 */
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.atomic.AtomicLong;

public class Atomic {
    static final AtomicLong count = new AtomicLong(0);
    static final int N = 1_000_000;

    public static void main(String[] args) {
        int nthreads = args.length > 0 ? Integer.parseInt(args[0]) : 4;

        long start = System.nanoTime();
        try (ExecutorService pool = Executors.newFixedThreadPool(nthreads)) {
            for (int t = 0; t < nthreads; t++) {
                pool.submit(() -> {
                    for (int i = 0; i < N; i++) {
                        count.incrementAndGet();    // load, add, store as one step
                    }
                });
            }
        }
        long ms = (System.nanoTime() - start) / 1_000_000;

        System.out.printf("Atomic        expected %,d  got %,d  (%d ms)%n",
                          (long) nthreads * N, count.get(), ms);
    }
}
