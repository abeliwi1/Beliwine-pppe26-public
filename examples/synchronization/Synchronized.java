/*
 * Synchronized.java -- a critical section with Java's built-in mutex
 *
 * Every Java object has a mutex (its monitor). synchronized (lock) { }
 * acquires lock's mutex on entry and releases it on exit, so at most one
 * thread at a time runs the block: the block is a critical section.
 *
 * All threads must synchronize on the same object. lock is static, so
 * there is one for the class. A static synchronized method does the same
 * thing, using the class object Synchronized.class as the lock:
 *
 *     static synchronized void increment() { count++; }
 *
 * Usage: java Synchronized.java [threads]     (default 4)
 */
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class Synchronized {
    static long count = 0;
    static final Object lock = new Object();  // one lock, shared by every task
    static final int N = 1_000_000;

    public static void main(String[] args) {
        int nthreads = args.length > 0 ? Integer.parseInt(args[0]) : 4;

        long start = System.nanoTime();
        try (ExecutorService pool = Executors.newFixedThreadPool(nthreads)) {
            for (int t = 0; t < nthreads; t++) {
                pool.submit(() -> {
                    for (int i = 0; i < N; i++) {
                        synchronized (lock) {     // acquire the mutex
                            count++;              // the critical section
                        }                         // release it
                    }
                });
            }
        }
        long ms = (System.nanoTime() - start) / 1_000_000;

        System.out.printf("Synchronized  expected %,d  got %,d  (%d ms)%n",
                          (long) nthreads * N, count, ms);
    }
}
