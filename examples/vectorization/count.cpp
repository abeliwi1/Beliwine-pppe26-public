// Count the elements of an array equal to a target.
//
//   g++ -O3 -mavx2 -std=c++17 count.cpp -o count
//   ./count
//
// This is the loop from Section 3 of the lecture, and unlike find_first it is
// one the compiler DOES vectorize.  Ask it:
//
//   $ g++ -O3 -mavx2 -fopt-info-vec -c count.cpp
//   count.cpp:...: optimized: loop vectorized using 32 byte vectors
//   count.cpp:...: optimized: loop vectorized using 16 byte vectors
//
// Three versions of the same function:
//
//   count_long  the plain loop, with a 64-bit (long) counter
//   count_int   the plain loop, with a 32-bit (int) counter -- one word changed
//   count_simd  hand-written intrinsics, accumulating in 32-bit lanes
//
// The compiler vectorizes count_long, but it must return exactly what the
// scalar loop would for every n, including an n large enough to overflow 32
// bits.  So it widens each compare result to 64 bits and accumulates four lanes
// per register instead of eight.  Change the counter to int and the widening
// disappears: g++ emits the same inner loop as count_simd.  Compare:
//
//   g++ -O3 -mavx2 -masm=intel -S -o - count.cpp | less
//
// clang goes further and unrolls the plain loops four ways with independent
// accumulators, which the hand-written loop does not do, so under clang
// count_int beats count_simd.  Build it with:
//
//   clang++ -O3 -mavx2 -std=c++17 -stdlib=libc++ count.cpp -o count
//
// count_simd is only correct because it assumes the answer fits in 32 bits --
// the same assumption count_int makes by its type, and count_long refuses to.

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


// ---------------------------------------------------------------------------
// Measurement harness, the same as find_first.cpp.  See the comments there:
// pin to one core, warm the clock up, and take the minimum of several trials.
// Set PIN_CPU to move off a busy core.
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

// The plain loop.  The compiler vectorizes it, but a 64-bit counter forces
// 64-bit accumulator lanes: four per 256-bit register, not eight.
__attribute__((noinline))
long count_long(const int* data, long n, int target) {
  long count = 0;
  for (long i = 0; i < n; i++) {
    if (data[i] == target) {
      count += 1;
    }
  }
  return count;
}

// One word different.  Now the compiler may accumulate in 32-bit lanes.
__attribute__((noinline))
long count_int(const int* data, long n, int target) {
  int count = 0;
  for (long i = 0; i < n; i++) {
    if (data[i] == target) {
      count += 1;
    }
  }
  return count;
}

#ifdef SIMD_ARCH_X86
// Hand-written AVX2.  A 256-bit register holds eight int32s.
__attribute__((noinline))
long count_simd(const int* data, long n, int target) {
  const __m256i wanted = _mm256_set1_epi32(target);
  __m256i acc = _mm256_setzero_si256();   // eight 32-bit running counts
  long clean_end = (n / 8) * 8;           // largest multiple of 8 <= n
  long i = 0;

  for (; i < clean_end; i += 8) {
    __m256i v = _mm256_loadu_si256((const __m256i*)(data + i));
    // Each lane: all 1s (that is, -1) if equal, else 0.  Subtracting -1 adds
    // one, so there is no mask-and-add -- the compare result IS the increment.
    __m256i eq = _mm256_cmpeq_epi32(v, wanted);
    acc = _mm256_sub_epi32(acc, eq);
  }

  // Add the eight lanes together, once, after the loop.
  uint32_t lanes[8];
  _mm256_storeu_si256((__m256i*)lanes, acc);
  long count = 0;
  for (int k = 0; k < 8; k++) count += lanes[k];

  // The tail: 0-7 leftover elements.
  for (; i < n; i++) {
    if (data[i] == target) count += 1;
  }
  return count;
}
#elif defined(SIMD_ARCH_NEON)
// Hand-written NEON.  A 128-bit register holds four int32s.
__attribute__((noinline))
long count_simd(const int* data, long n, int target) {
  const int32x4_t wanted = vdupq_n_s32(target);
  uint32x4_t acc = vdupq_n_u32(0);        // four 32-bit running counts
  long clean_end = (n / 4) * 4;           // largest multiple of 4 <= n
  long i = 0;

  for (; i < clean_end; i += 4) {
    int32x4_t v = vld1q_s32(data + i);
    uint32x4_t eq = vceqq_s32(v, wanted);  // each lane: all-1s if equal, else 0
    acc = vsubq_u32(acc, eq);              // subtracting all-1s adds one
  }

  long count = vaddvq_u32(acc);            // add the four lanes, once

  // The tail: 0-3 leftover elements.
  for (; i < n; i++) {
    if (data[i] == target) count += 1;
  }
  return count;
}
#endif

// ---------------------------------------------------------------------------

typedef long (*CountFn)(const int*, long, int);

static const int TRIALS = 7;

// Returns the best time per ELEMENT, in ns.
static double time_it(CountFn f, const int* data, long n, int target, int reps) {
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
    double ns = std::chrono::duration<double>(t1 - t0).count() / reps / n * 1e9;
    if (ns < best) best = ns;
  }
  return best;
}

int main() {
  int cpu = pin_to_fast_core();
  double ghz = warm_up();

  const long n = 16384;  // 64 KiB of int32
  const int target = 7;
  const int reps = 20000;

  // Values 0-15, so about one element in sixteen matches -- an unpredictable
  // branch for a scalar loop, if the compiler had left one in.
  std::vector<int> data((size_t)n);
  std::mt19937 rng(42);
  for (long i = 0; i < n; i++) data[(size_t)i] = (int)(rng() % 16);

  // All versions must agree, including on lengths that leave a tail.
  for (long m : {n, n - 1, n - 7, 5L, 0L}) {
    long a = count_long(data.data(), m, target);
    long b = count_int(data.data(), m, target);
    if (a != b) {
      printf("MISMATCH at n=%ld: long %ld, int %ld\n", m, a, b);
      return 1;
    }
#ifdef HAVE_SIMD_INTRINSICS
    long c = count_simd(data.data(), m, target);
    if (a != c) {
      printf("MISMATCH at n=%ld: long %ld, simd %ld\n", m, a, c);
      return 1;
    }
#endif
  }

  printf("cpu%d, %.2f GHz after warm-up (measured), min of %d trials\n", cpu, ghz, TRIALS);
  printf("%ld int32 values, %ld matches\n\n", n, count_long(data.data(), n, target));

  double base = time_it(count_long, data.data(), n, target, reps);
  printf("  %-24s %12s %9s\n", "version", "ns/element", "speedup");
  printf("  %-24s %12.4f %8.2fx\n", "compiler, long count", base, 1.0);
  double t = time_it(count_int, data.data(), n, target, reps);
  printf("  %-24s %12.4f %8.2fx\n", "compiler, int count", t, base / t);
#ifdef HAVE_SIMD_INTRINSICS
  t = time_it(count_simd, data.data(), n, target, reps);
  printf("  %-24s %12.4f %8.2fx\n", "hand-written intrinsics", t, base / t);
#endif
  return 0;
}
