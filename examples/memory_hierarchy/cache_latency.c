/* cache_latency.c -- LMBench-style pointer-chase latency vs working-set size.
 *
 * Modernized from the 1996 LMBench idea.  LMBench walked a fixed 128-byte
 * stride; every prefetcher built after about 2005 recognizes that instantly,
 * so a fixed stride now measures the prefetcher, not the cache.  We chase a
 * *random* permutation of cache lines instead: each load's address comes from
 * the previous load's result, so nothing can be issued early and the measured
 * time is true load-to-use latency.
 *
 * Three modes per working-set size:
 *   random  -- random permutation of 64B lines: defeats the prefetcher
 *   seq     -- lines in address order: the prefetcher hides the latency
 *   huge    -- random, on 2MB transparent huge pages: removes TLB misses
 *
 * Two measurement details matter on a mobile part like Strix Point:
 *   1. The core clock moves (3.5 GHz cold, 5.1 GHz boosted, less when hot).
 *      The core is warmed with high-IPC work before the run, and every data
 *      point is bracketed by a clock reading at each end -- see harness.h.
 *      The cycle column uses the mean and the csv carries the drift, so a
 *      moving clock is visible instead of silently corrupting the result.
 *   2. Modes are interleaved within each size, so thermal drift affects all
 *      three modes at a given size equally.
 *
 * Output is CSV on stdout: mode,bytes,ns,cycles,ghz
 *
 * Build: gcc -O2 -o cache_latency cache_latency.c
 */
#include "harness.h"
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#define LINE 64

static uint64_t rng_state = 88172645463325252ULL;
static uint64_t rng(void) {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 7;
    rng_state ^= rng_state << 17;
    return rng_state;
}

/* One cycle through all `n` lines of `buf`; the next pointer lives in the
 * first 8 bytes of each line. */
static void build_chain(char *buf, size_t n, int randomize) {
    size_t *order = malloc(n * sizeof(size_t));
    for (size_t i = 0; i < n; i++) order[i] = i;
    if (randomize)
        for (size_t i = n - 1; i > 0; i--) {   /* Fisher-Yates */
            size_t j = rng() % (i + 1);
            size_t t = order[i]; order[i] = order[j]; order[j] = t;
        }
    for (size_t i = 0; i < n; i++)
        *(void **)(buf + order[i] * LINE) = (void *)(buf + order[(i + 1) % n] * LINE);
    free(order);
}

static double chase(char *buf, size_t n, long accesses) {
    void **p = (void **)buf;
    for (size_t i = 0; i < n; i++) p = (void **)*p;   /* warm every line */

    /* best-of-N: contention can only ever make a pass slower, so the minimum
     * is the robust estimator.  More passes at the DRAM sizes, where another
     * process touching memory costs far more than it does in cache. */
    int reps = (n * 64 > (16UL << 20)) ? 7 : 3;
    double best = 1e30;
    for (int rep = 0; rep < reps; rep++) {
        double t0 = now_s();
        for (long i = 0; i < accesses; i++) p = (void **)*p;
        double ns = (now_s() - t0) * 1e9 / accesses;
        if (ns < best) best = ns;
    }
    __asm__ volatile ("" :: "r"(p));
    return best;
}

/* Ask /proc/self/smaps how much of this range the kernel actually backed with
 * 2MB pages.  MADV_HUGEPAGE is a request, not a guarantee. */
static long anon_huge_kb(void *addr) {
    FILE *f = fopen("/proc/self/smaps", "r");
    if (!f) return -1;
    char line[512];
    unsigned long lo, hi, want = (unsigned long)addr;
    int in_range = 0;
    long kb = -1;
    while (fgets(line, sizeof line, f)) {
        if (sscanf(line, "%lx-%lx", &lo, &hi) == 2)
            in_range = (want >= lo && want < hi);
        long v;
        if (in_range && sscanf(line, "AnonHugePages: %ld kB", &v) == 1) { kb = v; break; }
    }
    fclose(f);
    return kb;
}

int main(int argc, char **argv) {
    int cpu = (argc > 1) ? atoi(argv[1]) : 0;
    harness_begin(cpu, "cache_latency");

    printf("mode,bytes,ns,cycles,ghz,drift_pct\n");

    size_t sizes[64]; int nsizes = 0;
    for (size_t s = 8UL << 10; s <= 256UL << 20; s = s * 3 / 2)
        sizes[nsizes++] = (s / LINE) * LINE;

    for (int i = 0; i < nsizes; i++) {
        size_t bytes = sizes[i], n = bytes / LINE;
        if (n < 16) continue;

        for (int m = 0; m < 3; m++) {
            const char *name = (m == 0) ? "random" : (m == 1) ? "seq" : "huge";

            char *buf = NULL;
            if (posix_memalign((void **)&buf, 2UL << 20, bytes) != 0) {
                perror("posix_memalign"); return 1;
            }
            /* modes 0/1 explicitly refuse huge pages, so the "huge" delta
             * isolates page size rather than luck of the allocator */
            madvise(buf, bytes, (m == 2) ? MADV_HUGEPAGE : MADV_NOHUGEPAGE);
            memset(buf, 0, bytes);
            build_chain(buf, n, m != 1);

            long accesses = 20L * 1000 * 1000;
            if (bytes > (32UL << 20)) accesses = 4L * 1000 * 1000;
            if ((size_t)accesses < 4 * n) accesses = 4 * n;

            /* A reading at each end, not one before: the cycle count is only
             * as good as the assumption that the clock held across the chase. */
            clock_window w;
            w.before = measure_ghz(0.05);
            double ns = chase(buf, n, accesses);
            w.after  = measure_ghz(0.05);
            double ghz = cw_mean(w);
            printf("%s,%zu,%.3f,%.2f,%.3f,%+.1f\n",
                   name, bytes, ns, ns * ghz, ghz, cw_drift(w));
            fflush(stdout);
            if (cw_drift(w) > 3.0 || cw_drift(w) < -3.0)
                fprintf(stderr, "  clock moved %+.1f%% across %s/%zu KB\n",
                        cw_drift(w), name, bytes >> 10);

            if (m == 2 && bytes >= (4UL << 20))
                fprintf(stderr, "  %zu MB: AnonHugePages=%ld kB\n",
                        bytes >> 20, anon_huge_kb(buf));
            free(buf);
        }
    }
    return 0;
}
