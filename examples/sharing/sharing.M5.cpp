// sharing.M5.cpp -- what it costs two threads to share data, on an Apple M5.
//
//   ./sharing.M5 CORE_A CORE_B MODE [count]        CORE is P or E
//
// The macOS version of sharing.cpp: the same modes, but macOS can't pin a
// thread to a CPU, so each player asks for a kind of core instead.  A thread
// with QoS class BACKGROUND runs only on the efficiency cores; one with
// USER_INTERACTIVE runs on the performance cores when one is free.
//
// MODE is one of:
//
//   pause   true sharing.  Two threads share one atomic variable, alone on its
//           cache line, and take turns writing it: A waits until the value is
//           even and writes the next (odd) value, B waits until it is odd and
//           writes the next (even) one.  Every handoff moves the cache line
//           from one core to the other, so the time per handoff is the
//           latency of passing an update between them.  The waiting thread
//           runs the processor's spin-wait hint (Arm YIELD).
//   spin    the same, but the waiting thread re-reads the variable as fast as
//           it can, which keeps pulling the line away from the thread about to
//           write it.
//   false   false sharing.  Each thread increments its own volatile variable,
//           count times; the two variables sit side by side in one cache line.
//           Neither thread reads the other's data, but every write takes the
//           whole line away from the other core.
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
// The M5's cache line is 128 bytes (sysctl hw.cachelinesize), not the 64 of
// the Ryzen, so everything that must be alone on its line is aligned to 128.
//
// The two threads are Cilk workers: cilk_spawn runs player A on this worker,
// and the second worker steals the continuation and runs player B.  On an
// Apple M5 (4 performance cores sharing a 16 MB L2, 6 efficiency cores
// sharing a 6 MB L2):
//
//   two performance cores      ./sharing.M5 P P MODE    private L1s, one shared L2
//   two efficiency cores       ./sharing.M5 E E MODE    private L1s, the other shared L2
//   one of each                ./sharing.M5 P E MODE    no cache in common
//
// macOS can't say which core a thread ran on, so each player measures its own
// clock with a chain of dependent adds, and the program prints both: about
// 4.4 GHz is a performance core, well under that an efficiency core.
//
// Build: ~/opencilk/bin/clang++ -fopencilk -O3 -std=c++17 sharing.M5.cpp -o sharing.M5
// Run:   CILK_NWORKERS=2 ./sharing.M5 P P false
//        ./run.M5.sh runs every mode on all three pairs.
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cilk/cilk.h>
#include <cilk/cilk_api.h>
#include <pthread.h>
#include <sys/qos.h>

constexpr int TRIALS = 7;
constexpr int LINE = 128;                         // the M5's cache line
using Clock = std::chrono::steady_clock;

// ---- true sharing: one atomic variable, alone on its cache line ------------------

struct alignas(LINE) Shared {
    std::atomic<long> value{0};
};
static Shared shared;

// The processor's spin-wait hint: tells the core this thread is only waiting.
static inline void cpu_relax()
{
    __asm__ volatile("yield");
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

struct alignas(LINE) SameLine {                   // both counters in one line
    volatile long count[2];
};
struct alignas(LINE) OwnLine {                    // each counter alone on its line
    volatile long count;
};
static SameLine same_line;
static OwnLine own_line[2];

struct alignas(LINE) SameLineAtomic {
    std::atomic<long> count[2];
};
struct alignas(LINE) OwnLineAtomic {
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

// ---- the two players, as Cilk workers on chosen kinds of core -----------------------

// Ask for a performance core (P) or an efficiency core (E).
static void ask_for(char core)
{
    pthread_set_qos_class_self_np(core == 'P' ? QOS_CLASS_USER_INTERACTIVE : QOS_CLASS_BACKGROUND, 0);
}

// This thread's clock in GHz: a chain of dependent adds runs one per cycle.
static double clock_ghz()
{
    constexpr long ADDS = 20000000;
    long x = 0;
    auto t0 = Clock::now();
    for (long i = 0; i < ADDS / 8; i++)
        __asm__ volatile("add %0, %0, #1\n\tadd %0, %0, #1\n\tadd %0, %0, #1\n\tadd %0, %0, #1\n\t"
                         "add %0, %0, #1\n\tadd %0, %0, #1\n\tadd %0, %0, #1\n\tadd %0, %0, #1"
                         : "+r"(x));
    return ADDS / std::chrono::duration<double>(Clock::now() - t0).count() / 1e9;
}

enum Mode { PAUSE, SPIN, FALSE_SHARING, PADDED, FALSE_FENCE, PADDED_FENCE, FALSE_ATOMIC, PADDED_ATOMIC };
static const char* const MODE_NAMES[] = {
    "pause", "spin", "false", "padded", "false-fence", "padded-fence", "false-atomic", "padded-atomic"};
static Mode mode;
static double ghz_of[2];
static double seconds_of[2];
static std::atomic<int> arrived;

static void player(int me, char core, long count)
{
    ask_for(core);
    ghz_of[me] = clock_ghz();
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
static double run(char core_a, char core_b, long count)
{
    shared.value.store(0);
    arrived.store(0);
    cilk_spawn player(0, core_a, count);         // runs here, on this worker
    player(1, core_b, count);                    // the continuation: stolen by the other worker
    cilk_sync;
    return std::max(seconds_of[0], seconds_of[1]);
}

int main(int argc, char** argv)
{
    const char* name = argc > 3 ? argv[3] : "";
    int m = 0;
    while (m < 8 && strcmp(name, MODE_NAMES[m]))
        m++;
    bool cores_ok = argc > 2 && strlen(argv[1]) == 1 && strlen(argv[2]) == 1 && strchr("PE", argv[1][0]) &&
                    strchr("PE", argv[2][0]);
    if (argc < 4 || m == 8 || !cores_ok) {
        fprintf(stderr, "usage: CILK_NWORKERS=2 %s P|E P|E MODE [count]\n"
                        "  MODE: pause spin false padded false-fence padded-fence false-atomic padded-atomic\n",
                argv[0]);
        return 1;
    }
    char core_a = argv[1][0], core_b = argv[2][0];
    mode = Mode(m);
    bool handoffs = mode == PAUSE || mode == SPIN;
    // Enough work for about 0.1 s per trial at the cheapest.
    long count = argc > 4 ? atol(argv[4])
               : handoffs ? 2000000 : (mode == FALSE_SHARING || mode == PADDED) ? 100000000 : 10000000;
    if (__cilkrts_get_nworkers() < 2) {
        fprintf(stderr, "needs at least 2 Cilk workers (CILK_NWORKERS=2)\n");
        return 1;
    }

    run(core_a, core_b, count / 10);             // untimed: places the workers, warms the clocks
    double best = 1e30, ghz[2] = {0, 0};
    for (int t = 0; t < TRIALS; t++) {
        double s = run(core_a, core_b, count);
        if (s < best) {
            best = s;
            ghz[0] = ghz_of[0];
            ghz[1] = ghz_of[1];
        }
    }
    // A handoff is one write by either thread; an increment is one write by
    // one thread, while the other thread does its own increments alongside.
    printf("cores %c and %c, %-13s: %6.1f ns per %s (%ld, fastest of %d; clocks %.1f and %.1f GHz)\n",
           core_a, core_b, name, best * 1e9 / count, handoffs ? "handoff  " : "increment",
           count, TRIALS, ghz[0], ghz[1]);
    return 0;
}
