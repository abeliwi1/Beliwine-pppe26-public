// simpson.M5.cpp -- adaptive Simpson (1/3 rule) integration on [0, 1], six ways.
//
//   ./simpson <version> <input> [tolerance]
//
// versions:
//   serial       recursive adaptive Simpson (also the baseline for the others)
//   omp_static   [0,1] cut into PANELS equal panels, each integrated serially;
//                use schedule(static) and implement a thread-safe sum
//   omp_dynamic  the same with schedule(dynamic, 1)
//   omp_task     the recursion, with #pragma omp task / taskwait
//   cilk_for     the panels with cilk_for, with a thread-safe sum
//   cilk_for_g1  the same with #pragma cilk grainsize(1): one panel per leaf
//   cilk_spawn   the recursion, with cilk_spawn / cilk_sync
//
// inputs:
//   poly  a smooth polynomial: a Chebyshev series of degree 1024.  Chebyshev
//         polynomials wiggle fastest near the ends of the interval, so the
//         work is heaviest near x = 0 and x = 1
//   trig  a trigonometric polynomial, sum s_k sin(k w x) for k = 1..1024, with
//         pseudo-random signs s_k = +-1 and w = 2 pi * 1.37: it looks the same
//         everywhere on [0,1], so the work is even.  (With w = 2 pi every term
//         is 0 at x = 0, 1/2 and 1, and adaptive Simpson stops at once.)
//   exp   an exponential chirp, sin(K exp(A x)): its frequency grows like
//         exp(A x), and adaptive Simpson needs intervals in proportion to the
//         frequency, so the work grows exponentially toward x = 1
//
// Each run times the serial version first (before any parallel runtime
// starts), then the named version, and prints both and the speedup.
//
// This is the Apple M5 version of simpson.linux.cpp: the same integrands and
// parallel versions, with a harness for macOS, which can't pin threads to
// cores.  Instead the main thread and every OpenMP thread and Cilk worker ask
// for a performance core (QoS USER_INTERACTIVE).  Worker threads don't inherit
// the main thread's QoS, so each one asks for itself.
//
// Build (OpenCilk and Homebrew libomp):
//   ~/opencilk/bin/clang++ -fopencilk -O3 -std=c++17 \
//     -Xpreprocessor -fopenmp -I/opt/homebrew/opt/libomp/include \
//     -L/opt/homebrew/opt/libomp/lib -lomp simpson.M5.cpp -o simpson.M5
// Run on the M5's 4 performance cores:
//   OMP_NUM_THREADS=4 ./simpson.M5 omp_static poly
//   CILK_NWORKERS=4 ./simpson.M5 cilk_spawn exp
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <cilk/cilk.h>
#include <cilk/cilk_api.h>
#include <cilk/opadd_reducer.h>
#include <omp.h>
#include <pthread.h>
#include <sys/qos.h>

constexpr int PANELS = 1024;        // a power of two: panel p is the recursion's node at depth 10
constexpr int TRIALS = 5;
// Below this depth the recursive versions (omp_task, cilk_spawn) run serially.
// Too deep and the tasks are too small: at 24, omp_task fell to 1.1x on exp.
static int SPAWN_DEPTH = getenv("SPAWN_DEPTH") ? atoi(getenv("SPAWN_DEPTH")) : 12;

// ---- the integrands ----------------------------------------------------------

constexpr int CHEB = 1024;
static double cheb[CHEB + 1], trig[CHEB + 1];
static int input_kind;              // 0 = poly, 1 = exp, 2 = trig
static double CHIRP_K = 20, CHIRP_A = 8;

static double f(double x)
{
    if (input_kind == 0) {
        // Clenshaw's recurrence for sum c_k T_k(t), t = 2x - 1.
        double t = 2 * x - 1, b1 = 0, b2 = 0;
        for (int k = CHEB; k >= 1; k--) {
            double b0 = 2 * t * b1 - b2 + cheb[k];
            b2 = b1;
            b1 = b0;
        }
        return t * b1 - b2 + cheb[0];
    }
    if (input_kind == 2) {
        // Clenshaw's recurrence for sum c_k sin(k theta), theta = 2 pi x.
        double th = 2 * M_PI * 1.37 * x, c2 = 2 * std::cos(th), b1 = 0, b2 = 0;
        for (int k = CHEB; k >= 1; k--) {
            double b0 = c2 * b1 - b2 + trig[k];
            b2 = b1;
            b1 = b0;
        }
        return b1 * std::sin(th);
    }
    return std::sin(CHIRP_K * std::exp(CHIRP_A * x));
}

// ---- adaptive Simpson ------------------------------------------------------------

// Simpson's 1/3 rule on [a, b], from f at a, the midpoint and b.
static double simpson(double a, double b, double fa, double fm, double fb)
{
    return (b - a) / 6 * (fa + 4 * fm + fb);
}

// Accept [a, b] if its two halves agree with the whole to within 15 * eps;
// otherwise split, giving each half eps / 2.
static double adapt(double a, double b, double eps, double whole,
                    double fa, double fm, double fb)
{
    double m = (a + b) / 2;
    double flm = f((a + m) / 2), frm = f((m + b) / 2);
    double left = simpson(a, m, fa, flm, fm), right = simpson(m, b, fm, frm, fb);
    if (std::fabs(left + right - whole) <= 15 * eps)
        return left + right;
    return adapt(a, m, eps / 2, left, fa, flm, fm) +
           adapt(m, b, eps / 2, right, fm, frm, fb);
}

static double integrate(double a, double b, double eps)
{
    double fa = f(a), fm = f((a + b) / 2), fb = f(b);
    return adapt(a, b, eps, simpson(a, b, fa, fm, fb), fa, fm, fb);
}

// ---- the serial version: the starting point --------------------------------------------

// Integrate f over [0, 1] to within eps, one interval at a time.  Every
// parallel version below computes this same integral; each run times this
// function first, as the baseline for the speedup.
static double serial(double eps)
{
    return integrate(0, 1, eps);
}

// ---- the parallel versions ---------------------------------------------------------

static double panel(int p, double eps)
{
    return integrate(double(p) / PANELS, double(p + 1) / PANELS, eps / PANELS);
}

// TODO (Part 1): parallelize the loop over panels with OpenMP, static schedule.
//    also make updates to sum thread-safe
static double omp_static(double eps)
{
    double sum = 0;
    for (int p = 0; p < PANELS; p++)
        sum += panel(p, eps);
    return sum;
}

// TODO (Part 1): the same loop with OpenMP, dynamic schedule, one panel at a time.
//    also make updates to sum thread-safe
static double omp_dynamic(double eps)
{
    double sum = 0;
    for (int p = 0; p < PANELS; p++)
        sum += panel(p, eps);
    return sum;
}

// TODO (Part 1): the same loop with cilk_for and Cilk's default grain size.
//    also make updates to sum thread-safe
static double cilk_for_panels(double eps)
{
    double sum = 0;
    for (int p = 0; p < PANELS; p++)
        sum += panel(p, eps);
    return sum;
}

// TODO (Part 1): the same loop with cilk_for and a grain size of 1.
//    also make updates to sum thread-safe
static double cilk_for_grain1(double eps)
{
    double sum = 0;
    for (int p = 0; p < PANELS; p++)
        sum += panel(p, eps);
    return sum;
}

// TODO (Part 1): the recursion with OpenMP tasks.  Make the left half a task,
// compute the right half here, and wait for the task before adding them.
// Below SPAWN_DEPTH it is plain serial recursion.
static double adapt_omp(double a, double b, double eps, double whole,
                        double fa, double fm, double fb, int depth)
{
    // same logic as adapt()
    double m = (a + b) / 2;
    double flm = f((a + m) / 2), frm = f((m + b) / 2);
    double left = simpson(a, m, fa, flm, fm), right = simpson(m, b, fm, frm, fb);
    if (std::fabs(left + right - whole) <= 15 * eps)
        return left + right;

    // Below SPAWN_DEPTH, run serially to avoid too many tiny tasks.
    if (depth >= SPAWN_DEPTH)
        return adapt(a, m, eps / 2, left, fa, flm, fm) +
               adapt(m, b, eps / 2, right, fm, frm, fb);

    // TODO (Part 1): changes below here
    double l, r;
    l = adapt_omp(a, m, eps / 2, left, fa, flm, fm, depth + 1);
    r = adapt_omp(m, b, eps / 2, right, fm, frm, fb, depth + 1);
    return l + r;
}

// The team of threads starts here, and one of them starts the recursion;
// the others run the tasks it creates.
static double omp_task(double eps)
{
    double sum = 0;
    #pragma omp parallel
    #pragma omp single
    {
        double fa = f(0), fm = f(0.5), fb = f(1);
        sum = adapt_omp(0, 1, eps, simpson(0, 1, fa, fm, fb), fa, fm, fb, 0);
    }
    return sum;
}

// TODO (Part 1): the recursion with Cilk.  Spawn the left half, compute the
// right half here, and sync before adding them.
static double adapt_cilk(double a, double b, double eps, double whole,
                         double fa, double fm, double fb, int depth)
{
    // same logic as adapt()
    double m = (a + b) / 2;
    double flm = f((a + m) / 2), frm = f((m + b) / 2);
    double left = simpson(a, m, fa, flm, fm), right = simpson(m, b, fm, frm, fb);
    if (std::fabs(left + right - whole) <= 15 * eps)
        return left + right;

    // Below SPAWN_DEPTH, run serially to avoid too many tiny tasks.
    if (depth >= SPAWN_DEPTH)
        return adapt(a, m, eps / 2, left, fa, flm, fm) +
               adapt(m, b, eps / 2, right, fm, frm, fb);

    // TODO (Part 1): changes below here
    double l = adapt_cilk(a, m, eps / 2, left, fa, flm, fm, depth + 1);
    double r = adapt_cilk(m, b, eps / 2, right, fm, frm, fb, depth + 1);
    return l + r;
}

static double cilk_spawn_recursion(double eps)
{
    double fa = f(0), fm = f(0.5), fb = f(1);
    return adapt_cilk(0, 1, eps, simpson(0, 1, fa, fm, fb), fa, fm, fb, 0);
}

// ---- harness (from Activity 4's: warm-up, everything on the performance cores) -------

static void warm_up_clock()
{
    unsigned long long a = 1, b = 2, c = 3, d = 4;
    auto t0 = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - t0 < std::chrono::milliseconds(1500))
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

// The main thread, which runs the serial version, asks for a performance core.
static void ask_for_performance_core()
{
    pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
}

static void prefer_performance_cores_omp(int threads)
{
    // OpenMP keeps the same team for every parallel region with the same
    // number of threads, so asking once, in every thread of the team, is enough.
    #pragma omp parallel num_threads(threads)
    ask_for_performance_core();
}

static int prefer_performance_cores_cilk(int workers)
{
    // Cilk has no "every worker" construct.  Repeat a parallel loop, with
    // enough work per iteration that every worker steals some, until all of
    // them have asked.
    std::vector<char> asked(workers, 0);       // worker w writes only asked[w]
    for (int round = 0; round < 100; round++) {
        #pragma cilk grainsize(1)
        cilk_for (int i = 0; i < 16 * workers; i++) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
            int w = __cilkrts_get_worker_number();
#pragma clang diagnostic pop
            if (!asked[w]) {
                ask_for_performance_core();
                asked[w] = 1;
            }
            volatile double spin = 0;
            for (int k = 0; k < 100000; k++)
                spin = spin + k;
        }
        if (std::count(asked.begin(), asked.end(), 1) == workers)
            break;
    }
    return (int)std::count(asked.begin(), asked.end(), 1);
}

// run() returns its integral, which goes to a volatile before the clock is
// read again.  Without that, the serial version, which touches no memory the
// clock call could see, may be moved past the clock and time as 0 ms.
static volatile double sink;

template <class F> static double fastest(F run)
{
    sink = run();                            // untimed: warms caches and threads
    double best = 1e30;
    for (int t = 0; t < TRIALS; t++) {
        auto t0 = std::chrono::steady_clock::now();
        sink = run();
        best = std::min(best, std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count());
    }
    return best;
}

int main(int argc, char** argv)
{
    const char* version = argc > 1 ? argv[1] : "";
    const char* input   = argc > 2 ? argv[2] : "poly";
    double (*run)(double) =
        !strcmp(version, "serial") || !strcmp(version, "skew") ? nullptr :
        !strcmp(version, "omp_static")  ? omp_static :
        !strcmp(version, "omp_dynamic") ? omp_dynamic :
        !strcmp(version, "omp_task")    ? omp_task :
        !strcmp(version, "cilk_for")    ? cilk_for_panels :
        !strcmp(version, "cilk_for_g1") ? cilk_for_grain1 :
        !strcmp(version, "cilk_spawn")  ? cilk_spawn_recursion : (double (*)(double))1;
    if (run == (double (*)(double))1 || (strcmp(input, "poly") && strcmp(input, "exp") && strcmp(input, "trig"))) {
        fprintf(stderr, "usage: %s serial|skew|omp_static|omp_dynamic|omp_task|cilk_for|cilk_for_g1|cilk_spawn poly|trig|exp [tolerance]\n", argv[0]);
        return 1;
    }
    input_kind = !strcmp(input, "exp") ? 1 : !strcmp(input, "trig") ? 2 : 0;
    unsigned long long r = 12345;
    for (int k = 0; k <= CHEB; k++) {
        cheb[k] = 1.0 / (1 + k);
        r = r * 6364136223846793005ULL + 1442695040888963407ULL;
        trig[k] = (r >> 63) ? 1.0 / 32 : -1.0 / 32;
    }
    // Defaults chosen so that each input takes a few hundred ms serially.
    double eps = argc > 3 ? atof(argv[3]) : input_kind == 2 ? 1e-8 : 1e-12;

    ask_for_performance_core();
    warm_up_clock();
    double want = 0;
    double ts = fastest([&] { return want = serial(eps); });

    if (!run) {
        printf("%-12s %-5s eps %.0e: serial %8.2f ms, integral %.15g\n", version, input, eps, ts * 1e3, want);
        if (!strcmp(version, "skew")) {
            // Time every panel serially: how uneven is the work?
            std::vector<double> t(PANELS);
            for (int p = 0; p < PANELS; p++) {
                auto t0 = std::chrono::steady_clock::now();
                volatile double v = panel(p, eps);
                (void)v;
                t[p] = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
            }
            std::vector<double> s = t;
            std::sort(s.begin(), s.end());
            double total = 0, top = 0, q[4] = {0, 0, 0, 0};
            for (int p = 0; p < PANELS; p++) { total += t[p]; q[4 * p / PANELS] += t[p]; }
            for (int i = PANELS - PANELS / 100; i < PANELS; i++) top += s[i];
            printf("  per panel: min %.1f us, median %.1f us, max %.1f us; costliest 1%% of panels: %.0f%% of the work\n",
                   s[0] * 1e6, s[PANELS / 2] * 1e6, s[PANELS - 1] * 1e6, 100 * top / total);
            printf("  work by quarter of [0,1]: %.2f%% %.2f%% %.2f%% %.2f%%\n",
                   100 * q[0] / total, 100 * q[1] / total, 100 * q[2] / total, 100 * q[3] / total);
        }
        return 0;
    }
    bool omp = !strncmp(version, "omp", 3);
    int threads = omp ? omp_get_max_threads() : __cilkrts_get_nworkers();
    if (omp)
        prefer_performance_cores_omp(threads);
    else if (int asked = prefer_performance_cores_cilk(threads); asked != threads)
        fprintf(stderr, "warning: only %d of %d Cilk workers asked for a performance core\n", asked, threads);
    double got = 0;
    double tp = fastest([&] { return got = run(eps); });
    double rel = std::fabs(got - want) / std::fabs(want);
    printf("%-12s %-5s eps %.0e: serial %8.2f ms, parallel %8.2f ms, %d threads, speedup %5.2fx, rel. diff %.1e\n",
           version, input, eps, ts * 1e3, tp * 1e3, threads, ts / tp, rel);
    return 0;
}
