// How much does a wider vector actually buy you?
//
//   clang++ -O3 -std=c++17 -stdlib=libc++ lanes.cpp -o lanes           # arm64
//   clang++ -O3 -std=c++17 -stdlib=libc++ -mavx2 -mbmi -mbmi2 -mfma \
//       -mf16c -maes -mpclmul lanes.cpp -o lanes                       # x86
//   ./lanes
//
// find_first.cpp answers "is hand-written SIMD worth it" for ONE element type.
// This one answers a different question: holding the loop, the data and the
// footprint fixed, how does the speedup over scalar scale with the number of
// LANES?
//
// A vector register is a fixed number of BITS, so the only way to change the
// lane count without changing the machine is to change the element width.  On
// 128-bit NEON one register holds:
//
//     int64    2 lanes        int16    8 lanes
//     int32    4 lanes        int8    16 lanes
//
// and on 256-bit AVX2, twice each of those.  Highway's ScalableTag<T> gives
// the native lane count for whatever T and whatever target, so the same source
// covers every row of that table -- which is the point of using it here rather
// than writing four kernels by hand.
//
// The footprint is held at 64 KiB for every width, NOT the element count.  If
// the count were fixed instead, the int8 row would touch 8 KiB and the int64
// row 64 KiB, and the comparison would be measuring the cache as much as the
// lanes.  Holding bytes fixed means every row scans the same memory; only the
// number of elements in it, and therefore the lane count, changes.
//
// The headline row is the absent target, because that scans all 64 KiB and so
// is the one case where the work is identical across widths.  An early match
// is reported too, to show what fixed overhead does to the speedup when there
// is barely any work to amortise it over.

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>
#include <vector>

#if defined(__linux__)
  #include <sched.h>
#elif defined(__APPLE__)
  #include <pthread.h>
  #include <sys/qos.h>
#endif

#if __has_include(<hwy/highway.h>)
#define HAVE_HIGHWAY 1
#include <hwy/contrib/algo/find-inl.h>
#include <hwy/highway.h>
namespace hn = hwy::HWY_NAMESPACE;
#else
#error "lanes.cpp needs Google Highway; install it or see find_first.cpp"
#endif

// ---------------------------------------------------------------------------
// Measurement harness -- the same one find_first.cpp uses, and for the same
// reasons: pin to one core, spin until the governor raises the clock, and
// report the minimum of several passes rather than the mean of one.  See the
// comments there for why each of the three matters at this time scale.
// ---------------------------------------------------------------------------

static int pin_to_fast_core() {
  int cpu = 0;
  if (const char* e = getenv("PIN_CPU")) cpu = atoi(e);
#if defined(__linux__)
  cpu_set_t set;
  CPU_ZERO(&set);
  CPU_SET(cpu, &set);
  if (sched_setaffinity(0, sizeof set, &set) != 0)
    fprintf(stderr, "warning: could not pin to cpu%d\n", cpu);
#elif defined(__APPLE__)
  pthread_set_qos_class_self_np(
      cpu < 4 ? QOS_CLASS_USER_INTERACTIVE : QOS_CLASS_BACKGROUND, 0);
#endif
  return cpu;
}

static double core_clock_ghz() {
  const long n = 100000000;
  long a = 0;
  auto t0 = std::chrono::steady_clock::now();
  for (long i = 0; i < n / 8; i++)
#if defined(__aarch64__)
    asm volatile("add %0,%0,#1\n\tadd %0,%0,#1\n\tadd %0,%0,#1\n\tadd %0,%0,#1\n\t"
                 "add %0,%0,#1\n\tadd %0,%0,#1\n\tadd %0,%0,#1\n\tadd %0,%0,#1"
                 : "+r"(a));
#else
    asm volatile("addq $1,%0\n\taddq $1,%0\n\taddq $1,%0\n\taddq $1,%0\n\t"
                 "addq $1,%0\n\taddq $1,%0\n\taddq $1,%0\n\taddq $1,%0"
                 : "+r"(a));
#endif
  auto t1 = std::chrono::steady_clock::now();
  return n / std::chrono::duration<double>(t1 - t0).count() / 1e9;
}

static double warm_up(double seconds = 0.8) {
  long a0 = 0, a1 = 0, a2 = 0, a3 = 0, a4 = 0, a5 = 0, a6 = 0, a7 = 0;
  auto t0 = std::chrono::steady_clock::now();
  while (std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count() < seconds)
    for (int i = 0; i < 200000; i++)
#if defined(__aarch64__)
      asm volatile("add %0,%0,#1\n\tadd %1,%1,#1\n\tadd %2,%2,#1\n\tadd %3,%3,#1\n\t"
                   "add %4,%4,#1\n\tadd %5,%5,#1\n\tadd %6,%6,#1\n\tadd %7,%7,#1"
                   : "+r"(a0),"+r"(a1),"+r"(a2),"+r"(a3),"+r"(a4),"+r"(a5),"+r"(a6),"+r"(a7));
#else
      asm volatile("addq $1,%0\n\taddq $1,%1\n\taddq $1,%2\n\taddq $1,%3\n\t"
                   "addq $1,%4\n\taddq $1,%5\n\taddq $1,%6\n\taddq $1,%7"
                   : "+r"(a0),"+r"(a1),"+r"(a2),"+r"(a3),"+r"(a4),"+r"(a5),"+r"(a6),"+r"(a7));
#endif
  return core_clock_ghz();
}

// ---------------------------------------------------------------------------
// The two versions, one template each.  Identical control flow to
// find_first.cpp's int64 pair; only the element type is now a parameter.
// ---------------------------------------------------------------------------

template <typename T>
long find_scalar(const T* data, long n, T target) {
  for (long i = 0; i < n; i++) {
    if (data[i] == target) return i;
  }
  return -1;
}

template <typename T>
long find_highway(const T* data, long n, T target) {
  const hn::ScalableTag<T> d;
  size_t pos = hn::Find(d, target, data, (size_t)n);
  return pos == (size_t)n ? -1 : (long)pos;  // Find returns count when absent
}

static const int TRIALS = 7;

template <typename T>
static double time_it(long (*f)(const T*, long, T), const T* data, long n,
                      T target, int reps) {
  double best = 1e300;
  volatile long sink = 0;
  for (int t = 0; t < TRIALS; t++) {
    auto t0 = std::chrono::steady_clock::now();
    for (int r = 0; r < reps; r++) {
      asm volatile("" ::"r"(data) : "memory");
      sink += f(data, n, target);
    }
    auto t1 = std::chrono::steady_clock::now();
    double ns = std::chrono::duration<double>(t1 - t0).count() / reps * 1e9;
    if (ns < best) best = ns;
  }
  return best;
}

// 64 KiB for every width, so each row scans the same memory.
static const long BYTES = 64 * 1024;

// Hold element-visits per trial roughly constant too, so a narrow row does not
// take eight times as long to measure as a wide one.
static long reps_for(long n) {
  long r = (long)(1.6e8 / (double)n);
  return r < 200 ? 200 : r;
}

template <typename T>
static void run_width(const char* name, double* out_speedup_absent) {
  const long n = BYTES / (long)sizeof(T);
  const int reps = (int)reps_for(n);

  // Values 1..100 fit every width down to int8; the target sits outside that
  // range so "absent" really is absent, and is planted when we want a hit.
  std::vector<T> data((size_t)n);
  std::mt19937_64 rng(42);
  for (long i = 0; i < n; i++) data[(size_t)i] = (T)((rng() % 100) + 1);
  const T target = (T)127;

  const size_t lanes = hn::Lanes(hn::ScalableTag<T>());

  // Absent: scans all 64 KiB.  This is the row the lane story is told from.
  double s_abs = time_it<T>(find_scalar<T>, data.data(), n, target, reps);
  double h_abs = time_it<T>(find_highway<T>, data.data(), n, target, reps);

  // Early match at element 16, where there is almost no work to amortise the
  // vector path's fixed setup over.
  data[16] = target;
  double s_early = time_it<T>(find_scalar<T>, data.data(), n, target, reps);
  double h_early = time_it<T>(find_highway<T>, data.data(), n, target, reps);
  data[16] = (T)1;

  printf("  %-7s %6zu %9ld %11.1f %11.1f %8.2fx %10.2fx\n", name, lanes, n,
         s_abs, h_abs, s_abs / h_abs, s_early / h_early);
  *out_speedup_absent = s_abs / h_abs;
}

int main() {
  int cpu = pin_to_fast_core();
  double ghz = warm_up();

  printf("cpu%d, %.2f GHz after warm-up (measured), min of %d trials\n", cpu, ghz,
         TRIALS);
  printf("highway target: %s\n", hwy::TargetName(HWY_TARGET));
  printf("%ld KiB scanned per call at every width\n\n", BYTES / 1024);

  printf("  %-7s %6s %9s %11s %11s %8s %10s\n", "type", "lanes", "elements",
         "scalar ns", "hwy ns", "speedup", "at elem 16");
  printf("  %-7s %6s %9s %11s %11s %8s %10s\n", "-------", "-----", "--------",
         "----------", "----------", "-------", "---------");

  double sp[4];
  run_width<int64_t>("int64", &sp[0]);
  run_width<int32_t>("int32", &sp[1]);
  run_width<int16_t>("int16", &sp[2]);
  run_width<int8_t>("int8", &sp[3]);

  // Lanes double each row, so a speedup proportional to lanes would double
  // too.  Printing the ratio of consecutive speedups shows how far short of
  // that it falls -- 2.00 would be perfect scaling, 1.00 no gain at all.
  printf("\n  speedup ratio per doubling of lanes: ");
  for (int i = 1; i < 4; i++) printf("%.2f ", sp[i] / sp[i - 1]);
  printf("\n  core clock at end of run: %.2f GHz\n", core_clock_ghz());
  return 0;
}
