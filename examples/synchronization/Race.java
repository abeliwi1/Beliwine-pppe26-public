/*
 * Race.java -- a shared increment with no synchronization
 *
 * Each of nthreads tasks adds 1 to one shared counter N times. count++
 * is three steps -- load, add, store -- and two threads can interleave
 * them, so one thread's store overwrites the other's and an increment is
 * lost. The total comes out short, and by a different amount on every run.
 *
 * Usage: java Race.java [threads]     (default 4)
 */
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class Race {
    static long count = 0;                  // static: one copy, shared by every task
    static final int N = 1_000_000;

    public static void main(String[] args) {
        int nthreads = args.length > 0 ? Integer.parseInt(args[0]) : 4;

        long start = System.nanoTime();
        try (ExecutorService pool = Executors.newFixedThreadPool(nthreads)) {
            for (int t = 0; t < nthreads; t++) {
                pool.submit(() -> {
                    for (int i = 0; i < N; i++) {
                        count++;            // load, add, store: not one step
                    }
                });
            }
        }                                   // close() waits for every task
        long ms = (System.nanoTime() - start) / 1_000_000;

        System.out.printf("Race          expected %,d  got %,d  (%d ms)%n",
                          (long) nthreads * N, count, ms);
    }
}
