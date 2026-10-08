// scaling_bench.c -- a fixed-size problem, solved with 1..N threads.
//
// Build: gcc -O2 -fopenmp -o scaling_bench scaling_bench.c -lm
// Run:   ./scaling_bench > scaling.csv
//
// This is a STRONG scaling benchmark: the problem size never changes. Only the
// number of threads does. That is the setting Amdahl's law describes.
//
// The program has two phases, deliberately:
//
//   SERIAL   a linear recurrence  x[i] = a*x[i-1] + b.  Each step depends on
//            the previous one, so this phase cannot be parallelized at all --
//            it is the "1-p" of Amdahl's law, made concrete rather than
//            assumed. Its cost is set by --serial-steps.
//
//   PARALLEL a compute-bound map over a fixed array. The working set is small
//            enough to sit in L3, so this phase is limited by arithmetic
//            rather than by memory bandwidth. That matters: a bandwidth-bound
//            parallel phase stops scaling for reasons that have nothing to do
//            with the serial fraction, which would confound the lesson.
//
// Threads pin themselves to cpu 0,1,2,...  On a heterogeneous part that
// ordering is itself instructive -- see the notebook.

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sched.h>
#include <omp.h>
#include <unistd.h>

static double now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1e3 + ts.tv_nsec / 1e6;
}

// Force a value to be materialized here and now, so the optimizer cannot
// sink the loop that produced it past the timer call below.
static void sink(double v) { __asm__ volatile("" : : "x"(v) : "memory"); }

// Return v, but opaquely: the optimizer cannot see that the result is v, so
// no computation depending on it can be hoisted above this point.
static double opaque(double v) { __asm__ volatile("" : "+x"(v)); return v; }

// Pin the calling thread to one logical cpu.
static void pin_to(int cpu) {
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    sched_setaffinity(0, sizeof(set), &set);
}

// The parallel kernel: ~40 flops per element, no memory traffic beyond the
// one load and one store, so it is compute bound.
static double kernel(double v) {
    double acc = v;
    for (int k = 0; k < 10; k++)
        acc = acc * 0.999999 + 1e-9 * (double)k + acc * acc * 1e-12;
    return acc;
}

int main(int argc, char **argv) {
    // Set BEFORE the OpenMP runtime initializes. By default GOMP's idle worker
    // threads spin rather than sleep, and because every thread here is pinned
    // to a specific cpu, idle spinners from a previous (larger) parallel
    // region contend directly with the threads doing the work. Left alone,
    // that alone turns a 5x speedup at 16 threads into a 0.5x SLOWDOWN -- a
    // measurement artifact of the runtime, not a property of the program.
    // libgomp reads these in its constructor, before main runs, so setting
    // them here is too late -- we have to set them and re-exec ourselves.
    if (!getenv("SCALING_BENCH_REEXEC")) {
        setenv("OMP_WAIT_POLICY", "passive", 1);
        setenv("GOMP_SPINCOUNT", "0", 1);
        setenv("SCALING_BENCH_REEXEC", "1", 1);
        execv("/proc/self/exe", argv);
        perror("execv");                 // only reached if exec failed
        return 1;
    }

    long   n            = 1L << 20;   // 1 Mi doubles = 8 MB, L3 resident
    long   steps[8]     = {322000};   // recurrence lengths -> the serial phases
    int    nsteps       = 1;
    int    max_threads  = 12;         // physical cores on this part
    int    reps         = 5;
    const char *sched   = "static";   // static | dynamic | guided
    int    chunk        = 4096;       // chunk size for dynamic/guided
    int    cpus[64], ncpus = 0;       // explicit cpu list; default is 0,1,2,...
    int    init_cpu     = -1;         // cpu that first-touches the array (-1 = cpus[0])

    for (int i = 1; i < argc; i++) {
        if      (!strcmp(argv[i], "--n")            && i + 1 < argc) n            = atol(argv[++i]);
        else if (!strcmp(argv[i], "--serial-steps") && i + 1 < argc) {
            nsteps = 0;
            for (char *tok = strtok(argv[++i], ","); tok && nsteps < 8; tok = strtok(NULL, ","))
                steps[nsteps++] = atol(tok);
        }
        else if (!strcmp(argv[i], "--max-threads")  && i + 1 < argc) max_threads  = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--reps")         && i + 1 < argc) reps         = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--schedule")     && i + 1 < argc) sched        = argv[++i];
        else if (!strcmp(argv[i], "--chunk")        && i + 1 < argc) chunk        = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--init-cpu")     && i + 1 < argc) init_cpu = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--cpus")         && i + 1 < argc) {
            ncpus = 0;
            for (char *tok = strtok(argv[++i], ","); tok && ncpus < 64; tok = strtok(NULL, ","))
                cpus[ncpus++] = atoi(tok);
        }
        else { fprintf(stderr, "unknown arg: %s\n", argv[i]); return 1; }
    }

    // schedule(runtime) below reads whichever policy we set here. static gives
    // every thread an equal slice, which is optimal only if every core runs at
    // the same speed; dynamic hands out chunks on demand, so a fast core simply
    // takes more of them.
    // The chunk size matters as much as the policy: omp_set_schedule with a
    // chunk of 0 means chunk 1 for dynamic, which dispatches every single
    // iteration through the runtime and costs more than the kernel it is
    // scheduling -- a 0.2x "speedup".
    if (ncpus == 0) { for (int i = 0; i < 64; i++) cpus[i] = i; ncpus = 64; }
    if (max_threads > ncpus) max_threads = ncpus;

    omp_sched_t kind = omp_sched_static;
    int         ch   = 0;
    if      (!strcmp(sched, "dynamic")) { kind = omp_sched_dynamic; ch = chunk; }
    else if (!strcmp(sched, "guided"))  { kind = omp_sched_guided;  ch = chunk; }
    omp_set_schedule(kind, ch);

    double *x = malloc((size_t)n * sizeof *x);
    if (!x) { perror("malloc"); return 1; }

    // Warm up: bring the cores out of their idle clock state before anything
    // is timed. Without this the first configuration measured runs at a lower
    // frequency than the rest and its baseline T(1) is inflated -- which
    // silently corrupts every speedup computed against it.
    // Warm up for a fixed wall-clock duration, not a fixed iteration count.
    // A short warmup is not enough: the first process to run after the machine
    // has been idle executes about 1.4x slower than the next one, across both
    // phases, until the clocks ramp. Measured in ascending-order runs this
    // penalty lands entirely on whichever configuration happens to run first.
    double warm_start = now_ms();
    while (now_ms() - warm_start < 1500.0) {
        double sw = opaque(1.0);
        for (long i = 0; i < (1L << 20); i++) sw = sw * 0.9999999 + 1e-7;
        sink(sw);
        omp_set_num_threads(max_threads);
        #pragma omp parallel
        {
            pin_to(cpus[omp_get_thread_num()]);
            #pragma omp for schedule(runtime)
            for (long i = 0; i < n; i++) x[i] = kernel(x[i]);
        }
        sink(x[n - 1]);
    }

    printf("schedule,serial_steps,threads,rep,serial_ms,parallel_ms,total_ms\n");

    // Thread counts are visited in a SHUFFLED order within each repetition.
    // Measured in ascending order, t=1 always lands immediately after the
    // warmup while the chip is at its hottest and slowest, which inflates the
    // T(1) baseline and therefore deflates every speedup computed from it.
    // Shuffling spreads that transient across the whole curve instead.
    // Every (serial fraction, thread count) pair is measured inside ONE
    // process, in shuffled order. Run as separate processes, whichever
    // configuration starts first after the machine has been idle pays a ~1.4x
    // cold-clock penalty on BOTH phases, which makes absolute times
    // incomparable between configurations. Sharing one process removes it.
    int npairs = nsteps * max_threads;
    int cfg_of[8 * 64], thr_of[8 * 64];
    for (int c = 0; c < nsteps; c++)
        for (int i = 0; i < max_threads; i++) {
            cfg_of[c * max_threads + i] = c;
            thr_of[c * max_threads + i] = i + 1;
        }

    unsigned seed = 12345u;
    for (int r = 0; r < reps; r++) {
        for (int i = npairs - 1; i > 0; i--) {           // Fisher-Yates
            seed = seed * 1103515245u + 12345u;
            int j = (int)((seed >> 16) % (unsigned)(i + 1));
            int tc = cfg_of[i]; cfg_of[i] = cfg_of[j]; cfg_of[j] = tc;
            int tt = thr_of[i]; thr_of[i] = thr_of[j]; thr_of[j] = tt;
        }

        for (int k = 0; k < npairs; k++) {
            int t            = thr_of[k];
            long serial_steps = steps[cfg_of[k]];
            omp_set_num_threads(t);

            // First touch decides which L3 domain the array becomes resident in.
            pin_to(init_cpu >= 0 ? init_cpu : cpus[0]);
            for (long i = 0; i < n; i++) x[i] = 1.0 + (double)(i & 1023) * 1e-6;

            // ---- serial phase: a dependent chain, one thread, cpu 0 ----
            pin_to(cpus[0]);
            double t0 = now_ms();
            double s = opaque(1.0);
            for (long i = 0; i < serial_steps; i++)
                s = s * 0.9999999 + 1e-7;
            sink(s);
            double t1 = now_ms();

            // ---- parallel phase: the same fixed-size map, t threads ----
            #pragma omp parallel
            {
                pin_to(cpus[omp_get_thread_num()]);
                #pragma omp for schedule(runtime)
                for (long i = 0; i < n; i++)
                    x[i] = kernel(x[i]);
            }
            sink(x[n - 1]);
            double t2 = now_ms();

            printf("%s,%ld,%d,%d,%.3f,%.3f,%.3f\n", sched, serial_steps, t, r, t1 - t0, t2 - t1, t2 - t0);
            fflush(stdout);
        }
    }

    free(x);
    return 0;
}
