/* harness.h -- shared timing harness for the memory_hierarchy benchmarks.
 *
 * Three jobs, each with a trap behind it.
 *
 * 1. PIN.  cpu0-3 are Zen 5 (5.16 GHz), cpu4-11 are Zen 5c (3.29 GHz).  An
 *    unpinned run lands wherever the scheduler puts it.
 *
 * 2. WARM.  Under amd-pstate-epp the clock responds to how much work is
 *    retiring, so a core that has been idle starts near 3.5 GHz and a
 *    measurement taken there is a stable, reproducible, wrong number.
 *    warm_core() runs eight *independent* add chains -- roughly 4 ops/cycle
 *    on this machine -- rather than one dependent chain, so it presents the
 *    governor with high-IPC work.
 *
 * 3. MEASURE THE CLOCK.  measure_ghz() uses a *single dependent* chain: each
 *    add waits on the previous one, so it retires at exactly one per cycle by
 *    construction and the rate is the core clock.  The independent version
 *    must NOT be used for this -- it reports ~20 G adds/s on this part, which
 *    is throughput, not frequency.
 *
 * Nanosecond results never depend on any of this.  Cycle counts do, so
 * clock_window() brackets a measurement with a reading at each end and the
 * programs print the drift rather than hiding it.
 */
#ifndef HARNESS_H
#define HARNESS_H

#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <sched.h>
#include <string.h>

static inline double now_s(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + 1e-9 * t.tv_nsec;
}

static inline void pin_to(int cpu) {
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    sched_setaffinity(0, sizeof set, &set);
}

/* One dependent chain: 1 add per cycle, so adds/second == cycles/second. */
static inline double measure_ghz(double seconds) {
    long n = 200L * 1000 * 1000;
    for (;;) {
        uint64_t x = 0;
        double t0 = now_s();
        for (long i = 0; i < n; i++)
            __asm__ volatile ("addq $1, %0" : "+r"(x));
        double dt = now_s() - t0;
        __asm__ volatile ("" :: "r"(x));
        if (dt >= seconds) return n / dt / 1e9;
        n *= 2;
    }
}

/* Eight independent chains: high IPC, which is what the governor reacts to. */
static inline void warm_core(double seconds) {
    double t0 = now_s();
    while (now_s() - t0 < seconds) {
        uint64_t a=0,b=0,c=0,d=0,e=0,f=0,g=0,h=0;
        for (long i = 0; i < 20L * 1000 * 1000; i++) {
            __asm__ volatile ("addq $1, %0" : "+r"(a));
            __asm__ volatile ("addq $1, %0" : "+r"(b));
            __asm__ volatile ("addq $1, %0" : "+r"(c));
            __asm__ volatile ("addq $1, %0" : "+r"(d));
            __asm__ volatile ("addq $1, %0" : "+r"(e));
            __asm__ volatile ("addq $1, %0" : "+r"(f));
            __asm__ volatile ("addq $1, %0" : "+r"(g));
            __asm__ volatile ("addq $1, %0" : "+r"(h));
        }
        __asm__ volatile ("" :: "r"(a),"r"(b),"r"(c),"r"(d),"r"(e),"r"(f),"r"(g),"r"(h));
    }
}

/* A clock reading either side of a timed section. */
typedef struct { double before, after; } clock_window;

static inline double cw_mean(clock_window w)  { return 0.5 * (w.before + w.after); }
static inline double cw_drift(clock_window w) {                 /* percent */
    double m = cw_mean(w);
    return m > 0 ? 100.0 * (w.after - w.before) / m : 0.0;
}

/* Standard preamble: pin, warm, report where the clock settled, and record
 * what else the machine was doing.  These benchmarks measure shared
 * resources, so a run taken under load is not wrong so much as unrepeatable,
 * and the load belongs in the record next to the numbers. */
static inline double harness_begin(int cpu, const char *what) {
    pin_to(cpu);
    double cold = measure_ghz(0.05);
    warm_core(1.0);
    double warm = measure_ghz(0.05);

    double load = -1; long avail_mb = -1;
    FILE *f = fopen("/proc/loadavg", "r");
    if (f) { if (fscanf(f, "%lf", &load) != 1) load = -1; fclose(f); }
    f = fopen("/proc/meminfo", "r");
    if (f) { char k[64]; long v;
             while (fscanf(f, "%63s %ld kB\n", k, &v) == 2)
                 if (!strcmp(k, "MemAvailable:")) { avail_mb = v >> 10; break; }
             fclose(f); }

    fprintf(stderr, "%s: cpu%d, clock %.2f GHz cold -> %.2f GHz warmed"
                    " | load %.2f, %ld MB available\n",
            what, cpu, cold, warm, load, avail_mb);
    if (load > 0.75)
        fprintf(stderr, "  NOTE: machine is busy; large working sets will be noisy\n");
    return warm;
}

#endif /* HARNESS_H */
