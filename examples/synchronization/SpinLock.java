/*
 * SpinLock.java -- a mutex built from an atomic
 *
 * A lock is a shared flag: false when free, true when held. To acquire
 * it, a thread must see false and write true in one step, or two threads
 * could both see false and both enter. compareAndSet(false, true) is that
 * step, done by one atomic instruction; it fails if the flag is already
 * true, and the thread tries again. Thread.onSpinWait() is the pause hint
 * from Lecture 13: it keeps the waiting thread from hammering the line.
 *
 * synchronized and ReentrantLock are built the same way, with one more
 * step: a thread that waits too long asks the OS to put it to sleep.
 *
 * Usage: java SpinLock.java [threads]     (default 4)
 */
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.atomic.AtomicBoolean;

public class SpinLock {
    static long count = 0;
    static final AtomicBoolean held = new AtomicBoolean(false);
    static final int N = 1_000_000;

    static void lock() {
        while (!held.compareAndSet(false, true)) {  // free? then take it, in one step
            Thread.onSpinWait();                    // taken: pause, then try again
        }
    }

    static void unlock() {
        held.set(false);
    }

    public static void main(String[] args) {
        int nthreads = args.length > 0 ? Integer.parseInt(args[0]) : 4;

        long start = System.nanoTime();
        try (ExecutorService pool = Executors.newFixedThreadPool(nthreads)) {
            for (int t = 0; t < nthreads; t++) {
                pool.submit(() -> {
                    for (int i = 0; i < N; i++) {
                        lock();
                        count++;            // the critical section
                        unlock();
                    }
                });
            }
        }
        long ms = (System.nanoTime() - start) / 1_000_000;

        System.out.printf("SpinLock      expected %,d  got %,d  (%d ms)%n",
                          (long) nthreads * N, count, ms);
    }
}
