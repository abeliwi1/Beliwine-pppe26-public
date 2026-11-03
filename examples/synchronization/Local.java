/*
 * Local.java -- don't share: count locally, combine once
 *
 * Each task counts in a local variable, which no other thread can see,
 * and adds its total to the shared counter once, at the end. Only that
 * one add needs synchronization. This is what OpenMP's reduction and
 * Cilk's reducers do for you.
 *
 * Usage: java Local.java [threads]     (default 4)
 */
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class Local {
    static long count = 0;
    static final Object lock = new Object();
    static final int N = 1_000_000;

    public static void main(String[] args) {
        int nthreads = args.length > 0 ? Integer.parseInt(args[0]) : 4;

        long start = System.nanoTime();
        try (ExecutorService pool = Executors.newFixedThreadPool(nthreads)) {
            for (int t = 0; t < nthreads; t++) {
                pool.submit(() -> {
                    long mine = 0;          // declared in the task: private to it
                    for (int i = 0; i < N; i++) {
                        mine++;
                    }
                    synchronized (lock) {
                        count += mine;      // one critical section per task
                    }
                });
            }
        }
        long ms = (System.nanoTime() - start) / 1_000_000;

        System.out.printf("Local         expected %,d  got %,d  (%d ms)%n",
                          (long) nthreads * N, count, ms);
    }
}
