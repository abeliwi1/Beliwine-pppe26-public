/*
 * Volatile.java -- volatile is not synchronization
 *
 * volatile makes every read and write of count go to memory, so each
 * thread sees the others' writes. It does not make count++ one step:
 * the load, add and store can still interleave, and increments are
 * still lost. volatile gives visibility, not atomicity.
 *
 * Usage: java Volatile.java [threads]     (default 4)
 */
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class Volatile {
    static volatile long count = 0;         // every access goes to memory
    static final int N = 1_000_000;

    public static void main(String[] args) {
        int nthreads = args.length > 0 ? Integer.parseInt(args[0]) : 4;

        long start = System.nanoTime();
        try (ExecutorService pool = Executors.newFixedThreadPool(nthreads)) {
            for (int t = 0; t < nthreads; t++) {
                pool.submit(() -> {
                    for (int i = 0; i < N; i++) {
                        count++;            // still load, add, store
                    }
                });
            }
        }
        long ms = (System.nanoTime() - start) / 1_000_000;

        System.out.printf("Volatile      expected %,d  got %,d  (%d ms)%n",
                          (long) nthreads * N, count, ms);
    }
}
