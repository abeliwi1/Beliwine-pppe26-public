/*
 * Futures.java -- waiting for a result, not for a thread
 *
 * pool.submit() returns a Future: a handle to a result that may not exist
 * yet. future.get() waits until the task has finished and returns its
 * result, or throws the exception the task threw. It is Cilk's spawn and
 * sync, written as a value: submit is the spawn, get is the sync, and the
 * ordering comes with the data, so there is nothing shared to protect.
 *
 * A CompletableFuture goes one step further: instead of waiting for a
 * result, say what to do with it when it arrives. thenApply and
 * thenCombine build a graph of steps, and the pool runs each step once
 * its inputs are ready.
 *
 * Usage: java Futures.java [threads]   (default 4)
 */
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;

public class Futures {
    static final int N = 100_000_000;

    static long sumOfSquares(int lo, int hi) {
        long s = 0;
        for (int i = lo; i < hi; i++) s += (long) i * i % 1000;
        return s;
    }

    public static void main(String[] args) throws Exception {
        int nthreads = args.length > 0 ? Integer.parseInt(args[0]) : 4;
        long expected = sumOfSquares(0, N);

        try (ExecutorService pool = Executors.newFixedThreadPool(nthreads)) {
            int chunk = (N + nthreads - 1) / nthreads;

            // Future: submit every block, then wait for each block's result
            List<Future<Long>> parts = new ArrayList<>();
            for (int t = 0; t < nthreads; t++) {
                int lo = t * chunk, hi = Math.min(lo + chunk, N);
                parts.add(pool.submit(() -> sumOfSquares(lo, hi)));     // "spawn"
            }
            long total = 0;
            for (Future<Long> f : parts) total += f.get();              // "sync": waits for that block
            System.out.printf("Future             sum %,d  (%s)%n", total,
                              total == expected ? "correct" : "WRONG");

            // CompletableFuture: two halves, combined when both are done, then formatted
            int half = N / 2;
            CompletableFuture<Long> left = CompletableFuture.supplyAsync(() -> sumOfSquares(0, half), pool);
            CompletableFuture<Long> right = CompletableFuture.supplyAsync(() -> sumOfSquares(half, N), pool);
            CompletableFuture<String> report = left
                .thenCombine(right, Long::sum)                          // runs when both halves are ready
                .thenApply(s -> String.format("sum %,d  (%s)", s, s == expected ? "correct" : "WRONG"));
            System.out.println("CompletableFuture  " + report.get());
        }
    }
}
