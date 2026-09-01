import java.util.concurrent.RecursiveTask;
import java.util.concurrent.ForkJoinPool;

/*
 * ForkJoinSum.java -- java.util.concurrent fork/join basics
 *
 * Sums a large array by recursively splitting it in half: each half
 * becomes its own subtask, computed in parallel, then the two results
 * are joined and added. This is the fork/join counterpart to OpenMP's
 * reduction(+:sum) in ../openmp/reduction.c -- same idea (divide and
 * conquer reduction), different mechanism: OpenMP hands scheduling to
 * the compiler/runtime via a pragma, fork/join makes the recursion
 * explicit as tasks on a work-stealing pool.
 *
 * Usage: java ForkJoinSum.java [N] [threads] [threshold]
 *   (default N = 50_000_000, threads = Runtime.availableProcessors(), threshold = 10_000)
 */
public class ForkJoinSum extends RecursiveTask<Long> {
    static int THRESHOLD = 10_000; // below this, just add sequentially

    final long[] a;
    final int lo, hi;

    ForkJoinSum(long[] a, int lo, int hi) {
        this.a = a;
        this.lo = lo;
        this.hi = hi;
    }

    @Override
    protected Long compute() {
        if (hi - lo <= THRESHOLD) {
            long sum = 0;
            for (int i = lo; i < hi; i++) sum += a[i];
            return sum;
        }

        int mid = (lo + hi) / 2;
        ForkJoinSum left  = new ForkJoinSum(a, lo, mid);
        ForkJoinSum right = new ForkJoinSum(a, mid, hi);

        left.fork();                         // hand the left half to another thread
        long rightSum = right.compute();     // do the right half on this thread
        long leftSum  = left.join();         // wait for the left half's result

        return leftSum + rightSum;
    }

    static final int TRIALS = 4;

    public static void main(String[] args) {
        int n = args.length > 0 ? Integer.parseInt(args[0]) : 50_000_000;
        int threads = args.length > 1 ? Integer.parseInt(args[1]) : Runtime.getRuntime().availableProcessors();
        if (args.length > 2) THRESHOLD = Integer.parseInt(args[2]);

        long[] a = new long[n];
        for (int i = 0; i < n; i++) a[i] = i;

        System.out.printf("N = %,d  threads = %d  threshold = %,d%n%n", n, threads, THRESHOLD);

        long serialStart = System.nanoTime();
        long serialSum = 0;
        for (long x : a) serialSum += x;
        long serialTime = System.nanoTime() - serialStart;
        System.out.printf("serial    sum=%d  time=%.4f s%n", serialSum, serialTime / 1e9);

        // A sized pool (not the common pool) so parallelism can be varied per run.
        // Pool construction just allocates bookkeeping arrays sized by `threads`;
        // it does not eagerly spawn worker threads -- those start lazily on
        // first use, which is why we also time the first invoke() separately.
        long poolInitStart = System.nanoTime();
        ForkJoinPool pool = new ForkJoinPool(threads);
        long poolInitTime = System.nanoTime() - poolInitStart;
        System.out.printf("pool-init time=%.6f s%n", poolInitTime / 1e9);

        long firstCallStart = System.nanoTime();
        long firstSum = pool.invoke(new ForkJoinSum(a, 0, n)); // cold: worker threads spin up here
        long firstCallTime = System.nanoTime() - firstCallStart;
        System.out.printf("first-call sum=%d  time=%.6f s%n", firstSum, firstCallTime / 1e9);

        for (int t = 0; t < TRIALS; t++) {
            long parallelStart = System.nanoTime();
            long parallelSum = pool.invoke(new ForkJoinSum(a, 0, n));
            long parallelTime = System.nanoTime() - parallelStart;
            System.out.printf("forkjoin  sum=%d  time=%.4f s%n", parallelSum, parallelTime / 1e9);
        }

        pool.shutdown();
    }
}
