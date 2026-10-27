// sharing.cpp -- what it costs two threads to share data, on chosen CPUs.
//
//   ./sharing CPU_A CPU_B MODE [count]
//
// MODE is one of:
//
//   pause   true sharing.  Two threads share one atomic variable, alone on its
//           cache line, and take turns writing it: A waits until the value is
//           even and writes the next (odd) value, B waits until it is odd and
//           writes the next (even) one.  Every handoff moves the cache line
//           from one core to the other, so the time per handoff is the
//           latency of passing an update between them.  The waiting thread
//           runs the processor's spin-wait hint (x86 PAUSE, Arm YIELD).
//   spin    the same, but the waiting thread re-reads the variable as fast as
//           it can, which keeps pulling the line away from the thread about to
//           write it.
//   false   false sharing.  Each thread increments its own volatile variable,
//           count times; the two variables sit side by side in one 64-byte
//           cache line.  Neither thread reads the other's data, but every
//           write takes the whole line away from the other core.
//   padded  the same loop with each variable on its own cache line: what an
//           increment costs with nothing shared.
//
//   false-fence, padded-fence
//           the same two, with a full memory fence after every increment, so
//           each write must reach the cache (and own the line) before the next
//           one starts.  Without it, writes wait in the core's store buffer and
//           many commit together each time the core gets the line.
//   false-atomic, padded-atomic
//           the same two with std::atomic<long> counters and fetch_add, a
//           locked read-modify-write, which must own the line for every update.
//
// The two threads are Cilk workers: cilk_spawn runs player A on this worker,
// and the second worker steals the continuation and runs player B.  Each
// player pins its own worker thread to its CPU, so the two CPUs choose the
// case.  On a Ryzen AI 9 HX 370 (CPUs 0-3 Zen 5, 4-11 Zen 5c, 12-23 the
// second hardware thread of each core):
//
//   same core            ./sharing 0 12 MODE    the two hardware threads of one core
//   two Zen 5 cores      ./sharing 0 1  MODE    private L1/L2, one shared L3
//   two Zen 5c cores     ./sharing 4 5  MODE    private L1/L2, the other shared L3
//   Zen 5 and Zen 5c     ./sharing 0 4  MODE    no cache in common
//
// Build: ~/opencilk/bin/clang++ -fopencilk -O3 -std=c++17 sharing.cpp -o sharing
// Run:   CILK_NWORKERS=2 ./sharing 0 1 false
//        ./run.sh runs every mode on all four pairs.
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cilk/cilk.h>
#include <cilk/cilk_api.h>
#include <sched.h>

constexpr int TRIALS = 7;
using Clock = std::chrono::steady_clock;

// ---- true sharing: one atomic variable, alone on its cache line ------------------

struct alignas(64) Shared {
    std::atomic<long> value{0};
};
static Shared shared;

// The processor's spin-wait hint: tells the core this thread is only waiting.
static inline void cpu_relax()
{
#if defined(__x86_64__) || defined(__i386__)
    __builtin_ia32_pause();
#elif defined(__aarch64__)
    __asm__ volatile("yield");
#endif
}

// Player `me` (0 or 1) writes on its turn: values me+1, me+3, me+5, ...
// It waits for the other player's write before each of its own.
static void pingpong(int me, long handoffs, bool relax)
{
    for (long v = me; v < handoffs; v += 2) {
        while (shared.value.load(std::memory_order_acquire) != v)
            if (relax) cpu_relax();              // spin until it's my turn
        shared.value.store(v + 1, std::memory_order_release);
    }
}

// ---- false sharing: two private variables, together or apart -------------------------

struct alignas(64) SameLine {                     // both counters in one 64-byte line
    volatile long count[2];
};
struct alignas(64) OwnLine {                      // each counter alone on its line
    volatile long count;
};
static SameLine same_line;
static OwnLine own_line[2];

struct alignas(64) SameLineAtomic {
    std::atomic<long> count[2];
};
struct alignas(64) OwnLineAtomic {
    std::atomic<long> count;
};
static SameLineAtomic same_line_atomic;
static OwnLineAtomic own_line_atomic[2];

// Player `me` increments only its own counter.  volatile makes every
// increment a real load and store to memory, as a shared counter would be.
static void increment(volatile long* mine, long count)
{
    for (long i = 0; i < count; i++)
        *mine = *mine + 1;
}

// The same, but each write must complete before the next one starts.
static void increment_fenced(volatile long* mine, long count)
{
    for (long i = 0; i < count; i++) {
        *mine = *mine + 1;
        std::atomic_thread_fence(std::memory_order_seq_cst);
    }
}

// The same with an atomic read-modify-write.
static void increment_atomic(std::atomic<long>* mine, long count)
{
    for (long i = 0; i < count; i++)
        mine->fetch_add(1);
}

// ---- the two players, as Cilk workers on chosen CPUs -------------------------------

// Raise this core's clock before anything is timed.  The benchmark's own loops
// mostly wait (a pause loop barely uses the core), and on a Ryzen AI 9 HX 370 a
// core doing that stays at about 3.6 GHz instead of boosting to 5.1, so the
// same handoff measured 25.7 ns from an idle start and 17.9 ns right after
// other work had boosted the core.  Four independent integer chains keep the
// core busy enough to boost; a second is plenty.
static void warm_up_clock()
{
    unsigned long long a = 1, b = 2, c = 3, d = 4;
    auto t0 = Clock::now();
    while (Clock::now() - t0 < std::chrono::seconds(1))
        for (int k = 0; k < 10000; k++) {
            a = a * 6364136223846793005ULL + 1;
            b = b * 6364136223846793005ULL + 3;
            c ^= c << 13;
            c ^= c >> 7;
            d += c ^ (d >> 3);
        }
    volatile unsigned long long sink = a ^ b ^ c ^ d;
    (void)sink;
}

static void pin_to(int cpu)
{
    cpu_set_t one;
    CPU_ZERO(&one);
    CPU_SET(cpu, &one);
    if (sched_setaffinity(0, sizeof one, &one) != 0) {
        perror("sched_setaffinity");
        exit(1);
    }
}

enum Mode { PAUSE, SPIN, FALSE_SHARING, PADDED, FALSE_FENCE, PADDED_FENCE, FALSE_ATOMIC, PADDED_ATOMIC };
static const char* const MODE_NAMES[] = {
    "pause", "spin", "false", "padded", "false-fence", "padded-fence", "false-atomic", "padded-atomic"};
static Mode mode;
static int cpu_of[2];
static double seconds_of[2];
static std::atomic<int> arrived;

static bool warm_up;                              // set for the untimed first run

static void player(int me, int cpu, long count)
{
    pin_to(cpu);
    cpu_of[me] = sched_getcpu();
    if (warm_up)
        warm_up_clock();
    // Start together, so neither thread runs alone for part of its loop.
    arrived.fetch_add(1);
    while (arrived.load() < 2)
        cpu_relax();
    auto t0 = Clock::now();
    switch (mode) {
    case PAUSE:         pingpong(me, count, true); break;
    case SPIN:          pingpong(me, count, false); break;
    case FALSE_SHARING: increment(&same_line.count[me], count); break;
    case PADDED:        increment(&own_line[me].count, count); break;
    case FALSE_FENCE:   increment_fenced(&same_line.count[me], count); break;
    case PADDED_FENCE:  increment_fenced(&own_line[me].count, count); break;
    case FALSE_ATOMIC:  increment_atomic(&same_line_atomic.count[me], count); break;
    case PADDED_ATOMIC: increment_atomic(&own_line_atomic[me].count, count); break;
    }
    seconds_of[me] = std::chrono::duration<double>(Clock::now() - t0).count();
}

// One run: both players at once.  Returns the slower player's time.
static double run(int cpu_a, int cpu_b, long count)
{
    shared.value.store(0);
    arrived.store(0);
    cilk_spawn player(0, cpu_a, count);          // runs here, on this worker
    player(1, cpu_b, count);                     // the continuation: stolen by the other worker
    cilk_sync;
    return std::max(seconds_of[0], seconds_of[1]);
}

int main(int argc, char** argv)
{
    const char* name = argc > 3 ? argv[3] : "";
    int m = 0;
    while (m < 8 && strcmp(name, MODE_NAMES[m]))
        m++;
    if (argc < 4 || m == 8) {
        fprintf(stderr, "usage: CILK_NWORKERS=2 %s CPU_A CPU_B MODE [count]\n"
                        "  MODE: pause spin false padded false-fence padded-fence false-atomic padded-atomic\n",
                argv[0]);
        return 1;
    }
    int cpu_a = atoi(argv[1]), cpu_b = atoi(argv[2]);
    mode = Mode(m);
    bool handoffs = mode == PAUSE || mode == SPIN;
    // Enough work for about 0.1 s per trial at the cheapest.
    long count = argc > 4 ? atol(argv[4])
               : handoffs ? 2000000 : (mode == FALSE_SHARING || mode == PADDED) ? 100000000 : 10000000;
    if (__cilkrts_get_nworkers() < 2) {
        fprintf(stderr, "needs at least 2 Cilk workers (CILK_NWORKERS=2)\n");
        return 1;
    }

    warm_up = true;
    run(cpu_a, cpu_b, count / 10);               // untimed: pins the workers, raises the clocks
    warm_up = false;
    double best = 1e30;
    for (int t = 0; t < TRIALS; t++)
        best = std::min(best, run(cpu_a, cpu_b, count));
    if (cpu_of[0] != cpu_a || cpu_of[1] != cpu_b) {
        fprintf(stderr, "players ran on CPUs %d and %d, not %d and %d\n", cpu_of[0], cpu_of[1], cpu_a, cpu_b);
        return 1;
    }
    // A handoff is one write by either thread; an increment is one write by
    // one thread, while the other thread does its own increments alongside.
    printf("CPUs %2d and %2d, %-13s: %6.1f ns per %s (%ld, fastest of %d)\n",
           cpu_a, cpu_b, name, best * 1e9 / count, handoffs ? "handoff  " : "increment",
           count, TRIALS);
    return 0;
}
