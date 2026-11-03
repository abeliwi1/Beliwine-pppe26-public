/*
 * ExplicitLock.java -- a critical section with a Lock object
 *
 * ReentrantLock is the same mutex as synchronized, as a library object
 * with explicit lock() and unlock() calls. The calls are yours to make:
 * unlock() goes in a finally block so that an exception in the critical
 * section can't leave the lock held. In return you get what synchronized
 * can't do: tryLock() (give up if the lock is taken), a timeout, a fair
 * lock, and a lock and unlock in different methods.
 *
 * Usage: java ExplicitLock.java [threads]     (default 4)
 */
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.locks.ReentrantLock;

public class ExplicitLock {
    static long count = 0;
    static final ReentrantLock lock = new ReentrantLock();
    static final int N = 1_000_000;

    public static void main(String[] args) {
        int nthreads = args.length > 0 ? Integer.parseInt(args[0]) : 4;

        long start = System.nanoTime();
        try (ExecutorService pool = Executors.newFixedThreadPool(nthreads)) {
            for (int t = 0; t < nthreads; t++) {
                pool.submit(() -> {
                    for (int i = 0; i < N; i++) {
                        lock.lock();
                        try {
                            count++;        // the critical section
                        } finally {
                            lock.unlock();  // always runs, even on an exception
                        }
                    }
                });
            }
        }
        long ms = (System.nanoTime() - start) / 1_000_000;

        System.out.printf("ExplicitLock  expected %,d  got %,d  (%d ms)%n",
                          (long) nthreads * N, count, ms);
    }
}
