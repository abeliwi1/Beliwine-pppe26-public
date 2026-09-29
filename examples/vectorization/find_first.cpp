// Find the index of the first element in an array equal to a target.
//
//   clang++ -O3 -mavx2 -std=c++17 -stdlib=libc++ find_first.cpp -o find_first
//   ./find_first
//
// A third version using Google Highway is compiled in automatically if the
// headers are present.  Highway picks its instruction set from the -m flags, and
// a bare -mavx2 is not enough to select the AVX2 target, so build it with:
//
//   clang++ -O3 -std=c++17 -stdlib=libc++ -mavx2 -mbmi -mbmi2 -mfma -mf16c
//       -maes -mpclmul find_first.cpp -o find_first
//
// The program prints which target Highway actually selected, so you can see when
// you have got this wrong.
//
// This is a loop the compiler cannot vectorize, and it will tell you so:
//
//   $ clang++ -O3 -mavx2 -Rpass-analysis=loop-vectorize -c find_first.cpp
//   remark: loop not vectorized: could not determine number of loop iterations
//
// A vectorizer needs to know the trip count before the loop starts, so it can
// run whole vectors and then handle the remainder.  This loop exits when it
// finds something, so the trip count depends on the data.  The compiler emits
// pure scalar code -- one element per iteration, no vector instructions.
//
// Doing it by hand is legal because we only ever load elements that are inside
// the array.  We check a vector's worth at a time -- four int64 on AVX2, two on
// NEON -- and stop at the first vector containing a match, then work out which
// lane it was.  The answer is identical to the scalar version for every possible
// input: same index, same -1 when absent.  Nothing is assumed about n, about the
// values, or about how many matches there are.

#if defined(__x86_64__) || defined(__i386__)
#define HAVE_SIMD_INTRINSICS 1
#define SIMD_ARCH_X86 1
#include <immintrin.h>
#elif defined(__aarch64__) || defined(__arm__)
#define HAVE_SIMD_INTRINSICS 1
#define SIMD_ARCH_NEON 1
#include <arm_neon.h>
#endif

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <random>
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
#endif


// ---------------------------------------------------------------------------
// Measurement harness.
//
// These times are single-digit nanoseconds per call, so two machine properties
// that are easy to ignore will dominate the result if you let them:
//
//  1. HETEROGENEOUS CORES.  The Ryzen has Zen 5 cores at 5.13 GHz and Zen 5c
//     cores at 3.17 GHz; Apple silicon has performance and efficiency cores.
//     An unpinned run lands wherever the scheduler puts it, which is a 1.6x
//     spread on the same binary.
//  2. CLOCK RAMP.  The governor raises the clock in response to instructions
//     per cycle, not to the core being busy, and it takes a moment to do it.
//     A cold first measurement is taken at the base clock.
//
// Set PIN_CPU to move off a busy core.  On Apple silicon there is no numbered
// pinning, but the quality-of-service class selects the cluster: PIN_CPU under
// 4 asks for a performance core.
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

// A dependent chain of integer adds retires one per cycle, so this reports the
// clock the core is running at now rather than its nameplate maximum.
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

// Eight INDEPENDENT chains, so this is high-IPC and the governor responds to
// it.  Spinning on the dependent chain above does not raise the clock.
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

// Scalar.  The compiler leaves this alone.
long find_scalar(const int64_t* data, long n, int64_t target) {
  for (long i = 0; i < n; i++) {
    if (data[i] == target) return i;
  }
  return -1;
}

#ifdef SIMD_ARCH_X86
// Hand-written AVX2.  A 256-bit register holds four int64s.
long find_simd(const int64_t* data, long n, int64_t target) {
  const __m256i wanted = _mm256_set1_epi64x(target);
  long i = 0;

  // i + 4 <= n guarantees this load stays inside the array.
  for (; i + 4 <= n; i += 4) {
    __m256i v = _mm256_loadu_si256((const __m256i*)(data + i));
    __m256i eq = _mm256_cmpeq_epi64(v, wanted);

    // One bit per lane: bit k is set if lane k matched.
    int mask = _mm256_movemask_pd(_mm256_castsi256_pd(eq));
    if (mask) {
      // Lowest set bit = earliest matching lane, which is the first match.
      return i + __builtin_ctz(mask);
    }
  }

  // Fewer than four elements left over.
  for (; i < n; i++) {
    if (data[i] == target) return i;
  }
  return -1;
}
#elif defined(SIMD_ARCH_NEON)
// Hand-written NEON.  A 128-bit register holds two int64s.
long find_simd(const int64_t* data, long n, int64_t target) {
  const int64x2_t wanted = vdupq_n_s64(target);
  long i = 0;

  // i + 2 <= n guarantees this load stays inside the array.
  for (; i + 2 <= n; i += 2) {
    int64x2_t v = vld1q_s64(data + i);
    uint64x2_t eq = vceqq_s64(v, wanted);  // each lane: all-1s if equal, else 0

    // Only two lanes -- no cross-lane movemask needed, just check each.
    if (vgetq_lane_u64(eq, 0)) return i;
    if (vgetq_lane_u64(eq, 1)) return i + 1;
  }

  // Fewer than two elements left over.
  for (; i < n; i++) {
    if (data[i] == target) return i;
  }
  return -1;
}
#endif

#ifdef HAVE_HIGHWAY
// Portable SIMD.  hn::Find is the same algorithm as find_simd above -- whole
// vectors while they fit, then the remainder -- but written against Highway's
// abstract vector type instead of AVX2 intrinsics.
//
// ScalableTag<int64_t> means "however many int64 lanes this target has": four on
// AVX2, eight on AVX-512, two on SSE or NEON, and a length decided at runtime on
// SVE and RISC-V V.  Lanes(d) is not a constant, and the code does not care.
//
// The same source compiles for all of them.  Nothing here is x86.
long find_highway(const int64_t* data, long n, int64_t target) {
  const hn::ScalableTag<int64_t> d;
  size_t pos = hn::Find(d, target, data, (size_t)n);
  return pos == (size_t)n ? -1 : (long)pos;  // Find returns count when absent
}
#endif

// ---------------------------------------------------------------------------

typedef long (*FindFn)(const int64_t*, long, int64_t);

// Minimum of TRIALS passes, not the mean of one.  The mean of a single pass
// takes whatever interference happened to land during it, which is how two
// tables measured on the same machine end up disagreeing by a third.
static const int TRIALS = 7;

static double time_it(FindFn f, const int64_t* data, long n, int64_t target,
                      int reps) {
  double best = 1e300;
  volatile long sink = 0;
  for (int t = 0; t < TRIALS; t++) {
    auto t0 = std::chrono::steady_clock::now();
    for (int r = 0; r < reps; r++) {
      // Keeps the compiler from hoisting the call out of the timing loop.
      asm volatile("" ::"r"(data) : "memory");
      sink += f(data, n, target);
    }
    auto t1 = std::chrono::steady_clock::now();
    double ns = std::chrono::duration<double>(t1 - t0).count() / reps * 1e9;
    if (ns < best) best = ns;
  }
  return best;
}

int main() {
  int cpu = pin_to_fast_core();
  double ghz = warm_up();

  const long n = 8192;  // 64 KiB of int64, fits in L1
  const int64_t target = 7;
  const int reps = 20000;

  std::vector<int64_t> data((size_t)n);
  std::mt19937_64 rng(42);
  for (long i = 0; i < n; i++) data[(size_t)i] = (int64_t)(rng() % 1000000) + 1000;

  printf("cpu%d, %.2f GHz after warm-up (measured), min of %d trials\n", cpu, ghz, TRIALS);
  printf("%ld int64 values, searching for the first match\n", n);
#ifdef HAVE_HIGHWAY
  printf("highway target: %s, %zu lanes per vector\n\n",
         hwy::TargetName(HWY_TARGET), hn::Lanes(hn::ScalableTag<int64_t>()));
#else
  printf("(built without highway)\n\n");
#endif
#if defined(HAVE_SIMD_INTRINSICS) && defined(HAVE_HIGHWAY)
  printf("  %-10s %10s %10s %10s %9s %9s\n", "match at", "scalar", "simd",
         "highway", "simd", "highway");
#elif defined(HAVE_SIMD_INTRINSICS)
  printf("  %-10s %10s %10s %9s\n", "match at", "scalar", "simd", "simd");
#elif defined(HAVE_HIGHWAY)
  printf("  %-10s %10s %10s %9s\n", "match at", "scalar", "highway", "highway");
#else
  printf("  %-10s %10s\n", "match at", "scalar");
#endif

  const long positions[] = {0, 16, 256, 4096, 8191, -1};  // -1 == no match

  for (long p : positions) {
    std::vector<int64_t> probe = data;
    if (p >= 0) probe[(size_t)p] = target;

    // All versions must agree on every case, present or absent.
    long a = find_scalar(probe.data(), n, target);
#ifdef HAVE_SIMD_INTRINSICS
    long b = find_simd(probe.data(), n, target);
    if (a != b) {
      printf("  MISMATCH: scalar %ld, simd %ld\n", a, b);
      return 1;
    }
#endif
#ifdef HAVE_HIGHWAY
    long c = find_highway(probe.data(), n, target);
    if (a != c) {
      printf("  MISMATCH: scalar %ld, highway %ld\n", a, c);
      return 1;
    }
#endif

    double s = time_it(find_scalar, probe.data(), n, target, reps);
#ifdef HAVE_SIMD_INTRINSICS
    double v = time_it(find_simd, probe.data(), n, target, reps);
#endif
#ifdef HAVE_HIGHWAY
    double h = time_it(find_highway, probe.data(), n, target, reps);
#endif

    char label[24];
    if (p < 0) snprintf(label, sizeof label, "none");
    else snprintf(label, sizeof label, "%ld", p);

#if defined(HAVE_SIMD_INTRINSICS) && defined(HAVE_HIGHWAY)
    printf("  %-10s %9.1f %9.1f %9.1f %8.2fx %8.2fx\n", label, s, v, h, s / v,
           s / h);
#elif defined(HAVE_SIMD_INTRINSICS)
    printf("  %-10s %9.1f %9.1f %8.2fx\n", label, s, v, s / v);
#elif defined(HAVE_HIGHWAY)
    printf("  %-10s %9.1f %9.1f %8.2fx\n", label, s, h, s / h);
#else
    printf("  %-10s %9.1f\n", label, s);
#endif
  }
  return 0;
}
