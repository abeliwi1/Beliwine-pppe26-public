/*
 * Barrier.java -- threads that work in steps, and wait for each other between them
 *
 * Smooth an array: in every step, each element becomes the average of
 * itself and its two neighbours from the step before. Each thread owns one
 * block of the array, but the elements at the ends of its block need its
 * neighbours' values. So no thread may start step s+1 until every thread
 * has finished step s.
 *
 * A CyclicBarrier(n) makes that wait. Each thread calls await() when it
 * has finished its step, and await() returns, in all n threads, only when
 * the last of them has arrived. The barrier then resets itself for the
 * next step ("cyclic"). Its optional action runs once per step, after
 * everyone has arrived and before anyone leaves: here, it swaps the old
 * and new arrays.
 *
 * OpenMP has the same thing (#pragma omp barrier), and puts one at the end
 * of every parallel region: in Lecture 12, that is what kept pass 2 of the
 * prefix sum from starting before pass 1 was done.
 *
 * Without the barrier, a thread could read a neighbour's element before or
 * after the neighbour updated it, and the result would change from run to
 * run. Run with "nobarrier" to see it.
 *
 * Usage: java Barrier.java [threads] [nobarrier]   (default 4)
 */
import java.util.Arrays;
import java.util.concurrent.CyclicBarrier;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class Barrier {
    static final int N = 1_000_000;
    static final int STEPS = 100;

    static double[] old = new double[N + 2];    // one boundary element at each end
    static double[] next = new double[N + 2];

    static void smooth(double[] from, double[] to, int lo, int hi) {
        for (int i = lo; i < hi; i++)
            to[i] = (from[i - 1] + from[i] + from[i + 1]) / 3.0;
    }

    static void init(double[] a) {
        Arrays.fill(a, 0.0);
        for (int i = 1; i <= N; i++) a[i] = (i * 7919L) % 1000;   // something to smooth
    }

    public static void main(String[] args) throws Exception {
        int nthreads = args.length > 0 ? Integer.parseInt(args[0]) : 4;
        boolean useBarrier = !(args.length > 1 && args[1].equals("nobarrier"));

        // the serial answer, to check against
        init(old);
        double[] a = old.clone(), b = new double[N + 2];
        for (int s = 0; s < STEPS; s++) {
            smooth(a, b, 1, N + 1);
            double[] t = a; a = b; b = t;
        }
        double[] expected = a;

        // the parallel version: one block per thread, a barrier after every step
        init(old);
        Arrays.fill(next, 0.0);
        CyclicBarrier barrier = new CyclicBarrier(nthreads, () -> {
            double[] t = old; old = next; next = t;    // runs once, when all have arrived
        });

        try (ExecutorService pool = Executors.newFixedThreadPool(nthreads)) {
            int chunk = (N + nthreads - 1) / nthreads;
            for (int t = 0; t < nthreads; t++) {
                int lo = 1 + t * chunk, hi = Math.min(lo + chunk, N + 1);
                pool.submit(() -> {
                    for (int s = 0; s < STEPS; s++) {
                        if (useBarrier) {
                            smooth(old, next, lo, hi);
                            barrier.await();            // wait for every block's step s
                        } else {
                            // no barrier: each thread swaps its own view of the arrays,
                            // and reads its neighbours' elements whenever it gets to them
                            double[] from = (s % 2 == 0) ? old : next;
                            double[] to = (s % 2 == 0) ? next : old;
                            smooth(from, to, lo, hi);
                        }
                    }
                    return null;
                });
            }
        }
        double[] got = useBarrier ? old : (STEPS % 2 == 0 ? old : next);

        double worst = 0;
        for (int i = 1; i <= N; i++) worst = Math.max(worst, Math.abs(got[i] - expected[i]));
        System.out.printf("Barrier  %d threads, %s: largest difference from serial %.3g  (%s)%n",
                          nthreads, useBarrier ? "barrier" : "no barrier", worst,
                          worst == 0 ? "correct" : "WRONG");
    }
}
