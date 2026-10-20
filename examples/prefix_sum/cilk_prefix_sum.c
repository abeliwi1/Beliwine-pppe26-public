#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdatomic.h>
#include <sys/time.h>
#include <cilk/cilk.h>
#include <cilk/cilk_api.h>

/*
 *  cilk_prefix_sum.c
 *
 *  Parallel prefix sum using OpenCilk block parallelism (cilk_for).
 *
 *  Mirrors the OpenMP three-pass block approach but uses Cilk primitives.
 *  cilk_for divides the iteration space into blocks and schedules them
 *  using Cilk's work-stealing scheduler rather than OpenMP's thread pool.
 *
 *  Three passes over NBLOCKS contiguous chunks:
 *    Pass 1 (cilk_for): each block computes its local prefix sum in parallel.
 *    Pass 2 (serial):   prefix sum of the NBLOCKS totals to get block offsets.
 *    Pass 3 (cilk_for): each block adds its offset to its local prefix sums.
 *
 *  Work:  O(N)
 *  Span:  O(N/P + NBLOCKS)  where P = number of workers
 *
 *  Compile (OpenCilk):
 *    ~/opencilk/bin/clang -fopencilk -O3 cilk_prefix_sum.c -o cilk_prefix_sum
 *
 *  Usage: ./cilk_prefix_sum [N] [nblocks]   (defaults: N=100000000, nblocks=10)
 */

int N       = 100000000;
int NBLOCKS = 10;
const int TRIALS = 4;

int timeval_subtract(struct timeval *result, struct timeval *y, struct timeval *x)
{
    if (x->tv_usec < y->tv_usec) {
        int nsec = (y->tv_usec - x->tv_usec) / 1000000 + 1;
        y->tv_usec -= 1000000 * nsec;
        y->tv_sec  += nsec;
    }
    if (x->tv_usec - y->tv_usec > 1000000) {
        int nsec = (x->tv_usec - y->tv_usec) / 1000000;
        y->tv_usec += 1000000 * nsec;
        y->tv_sec  -= nsec;
    }
    result->tv_sec  = x->tv_sec  - y->tv_sec;
    result->tv_usec = x->tv_usec - y->tv_usec;
    return x->tv_sec < y->tv_sec;
}

/* Two cilk_for loops (passes 1 and 3) around the serial pass 2.

   Pass 1 carries the running sum in a local variable, s, rather than writing
   out[i] = out[i-1] + in[i].  The serial loop below can use that form because
   the compiler keeps out[i-1] in a register.  Inside a cilk_for body it
   doesn't: it reloads out[i-1] from memory every iteration, so each addition
   waits for the previous store to come back, and one worker ran 4.3x slower
   than serial (221 ms vs. 51 ms). */
void prefix_cilk(double *in, double *out)
{
    int chunk = (N + NBLOCKS - 1) / NBLOCKS;
    double totals[NBLOCKS];

    cilk_for (int b = 0; b < NBLOCKS; b++) {
        int lo = b * chunk;
        int hi = lo + chunk < N ? lo + chunk : N;
        if (lo >= N) { totals[b] = 0.0; continue; }
        double s = 0.0;
        for (int i = lo; i < hi; i++) {
            s += in[i];
            out[i] = s;
        }
        totals[b] = s;
    }

    for (int b = 1; b < NBLOCKS; b++)
        totals[b] += totals[b-1];

    cilk_for (int b = 1; b < NBLOCKS; b++) {
        int lo = b * chunk;
        int hi = lo + chunk < N ? lo + chunk : N;
        if (lo >= N) continue;
        double offset = totals[b-1];
        for (int i = lo; i < hi; i++)
            out[i] += offset;
    }
}

/*
 * Single cilk_for with a software barrier between passes 1 and 3.
 * The same worker that runs pass 1 for block b runs pass 3 for block b,
 * so its cache lines are still warm.
 *
 * The last worker to finish pass 1 runs the serial pass 2 (nblocks additions
 * — nanoseconds), then signals all others via an atomic flag.
 *
 * Spinning is only safe if every block is running at the same time on its
 * own worker; otherwise a spinner waits on a block that can never start and
 * the program hangs. Cilk does not promise that, so two guards enforce it:
 *   - nblocks is capped at the worker count, so there is a worker for each
 *     block (a spinner never steals, but at least one worker is always free
 *     to steal the next unstarted block);
 *   - grainsize(1) makes every block its own leaf, so no worker runs two
 *     blocks back to back and spins in the first.
 */
void prefix_cilk_barrier(double *in, double *out)
{
    int nworkers = __cilkrts_get_nworkers();
    int nblocks  = NBLOCKS < nworkers ? NBLOCKS : nworkers;
    int chunk = (N + nblocks - 1) / nblocks;
    double totals[nblocks];
    _Atomic int n_ready   = 0;
    _Atomic int pass2_done = 0;

    #pragma cilk grainsize(1)
    cilk_for (int b = 0; b < nblocks; b++) {
        int lo = b * chunk;
        int hi = lo + chunk < N ? lo + chunk : N;

        /* Pass 1: local prefix sum (an empty block still joins the barrier) */
        if (lo < N) {
            double s = 0.0;               /* running sum in a register; see prefix_cilk */
            for (int i = lo; i < hi; i++) {
                s += in[i];
                out[i] = s;
            }
            totals[b] = s;
        } else {
            totals[b] = 0.0;
        }

        /* Software barrier */
        if (atomic_fetch_add_explicit(&n_ready, 1, memory_order_acq_rel) == nblocks - 1) {
            for (int b2 = 1; b2 < nblocks; b2++)
                totals[b2] += totals[b2-1];
            atomic_store_explicit(&pass2_done, 1, memory_order_release);
        } else {
            while (!atomic_load_explicit(&pass2_done, memory_order_acquire))
                ;
        }

        /* Pass 3: add offset */
        if (b > 0 && lo < N) {
            double offset = totals[b-1];
            for (int i = lo; i < hi; i++)
                out[i] += offset;
        }
    }
}

void prefix_serial(double *in, double *out)
{
    out[0] = in[0];
    for (int i = 1; i < N; i++)
        out[i] = out[i-1] + in[i];
}

int verify(double *a, double *b)
{
    for (int i = 0; i < N; i++) {
        double rel_err = fabs(a[i] - b[i]) / fabs(a[i]);
        if (rel_err > 1e-10) return 0;
    }
    return 1;
}

#define TIME(label, call) do { \
    gettimeofday(&begin, NULL); \
    call; \
    gettimeofday(&end, NULL); \
    timeval_subtract(&tresult, &begin, &end); \
    double secs = (double)tresult.tv_sec + (double)tresult.tv_usec/1000000.0; \
    printf("%-22s  time=%.4f s  GB/s=%.2f\n", label, secs, \
           2.0*N*sizeof(double) / secs / 1e9); \
} while(0)

int main(int argc, char *argv[])
{
    if (argc > 1) N       = atoi(argv[1]);
    if (argc > 2) NBLOCKS = atoi(argv[2]);
    printf("N = %d  blocks = %d\n\n", N, NBLOCKS);

    double *in    = malloc(N * sizeof(double));
    double *out_s = malloc(N * sizeof(double));
    double *out_c = malloc(N * sizeof(double));

    for (int i = 0; i < N; i++)
        in[i] = (double)rand() / RAND_MAX;

    // warm up
    prefix_serial(in, out_s);
    prefix_cilk(in, out_c);
    printf("results match: %s\n\n", verify(out_s, out_c) ? "yes" : "NO");

    struct timeval begin, end, tresult;

    prefix_cilk_barrier(in, out_c);
    printf("results match (barrier): %s\n\n", verify(out_s, out_c) ? "yes" : "NO");

    printf("%-22s  %s\n", "version", "time       GB/s");
    printf("%-22s  %s\n", "-------", "----       ----");
    for (int t = 0; t < TRIALS; t++) TIME("serial",           prefix_serial(in, out_s));
    for (int t = 0; t < TRIALS; t++) TIME("cilk_for",         prefix_cilk(in, out_c));
    for (int t = 0; t < TRIALS; t++) TIME("cilk_for+barrier", prefix_cilk_barrier(in, out_c));

    free(in); free(out_s); free(out_c);
    return 0;
}
