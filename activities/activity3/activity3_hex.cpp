// Activity 3 -- Vectorizing a table lookup: hex encoding with a byte shuffle.
// STARTER.  Rename to activity3_hex.cpp.  You write to_hex_simd (Part 3) for
// your machine and to_hex_highway (Part 4).  Change nothing else.
//
// Hex-encode a byte array: each input byte becomes two ASCII characters,
// high nibble first, so 0x7a becomes "7a".
//
// Build (x86-64, Linux):
//   g++ -O3 -std=c++17 -mavx2 -mbmi -mbmi2 -mfma -mf16c -maes -mpclmul
//       activity3_hex.cpp -o activity3 -lhwy
//
// Build (Apple silicon, Homebrew Highway):
//   clang++ -O3 -std=c++17 -I/opt/homebrew/include -L/opt/homebrew/lib
//       activity3_hex.cpp -o activity3 -lhwy
//
// Highway is optional: without it the program builds and runs the other two
// versions.  On x86 a bare -mavx2 is NOT enough for Highway to select its AVX2
// target -- it needs the full flag list above.  The program prints the target
// Highway chose; check it.

#if defined(__x86_64__) || defined(__i386__)
#define SIMD_ARCH_X86 1
#include <immintrin.h>
#elif defined(__aarch64__)
#define SIMD_ARCH_NEON 1
#include <arm_neon.h>
#endif

#if __has_include(<hwy/highway.h>)
#define HAVE_HIGHWAY 1
#include <hwy/highway.h>
namespace hn = hwy::HWY_NAMESPACE;
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
// Measurement harness -- do not modify.  Request core placement, warm up,
// take the minimum of several trials.  Set PIN_CPU to move to another core.
// ---------------------------------------------------------------------------

static int pin_to_fast_core() {
  int cpu = 0;
  if (const char* e = getenv("PIN_CPU")) cpu = atoi(e);
#if defined(__linux__)
  cpu_set_t set;
  CPU_ZERO(&set);
  if (cpu < 0 || cpu >= CPU_SETSIZE) {
    fprintf(stderr, "warning: PIN_CPU outside valid affinity range; using cpu0\n");
    cpu = 0;
  }
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
#else  // {AT&T|Intel}: builds under -masm=intel as well as the default
    asm volatile("{addq $1,%0|add %0,1}\n\t{addq $1,%0|add %0,1}\n\t{addq $1,%0|add %0,1}\n\t{addq $1,%0|add %0,1}\n\t"
                 "{addq $1,%0|add %0,1}\n\t{addq $1,%0|add %0,1}\n\t{addq $1,%0|add %0,1}\n\t{addq $1,%0|add %0,1}"
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
      asm volatile("{addq $1,%0|add %0,1}\n\t{addq $1,%1|add %1,1}\n\t{addq $1,%2|add %2,1}\n\t{addq $1,%3|add %3,1}\n\t"
                   "{addq $1,%4|add %4,1}\n\t{addq $1,%5|add %5,1}\n\t{addq $1,%6|add %6,1}\n\t{addq $1,%7|add %7,1}"
                   : "+r"(a0),"+r"(a1),"+r"(a2),"+r"(a3),"+r"(a4),"+r"(a5),"+r"(a6),"+r"(a7));
#endif
  return core_clock_ghz();
}

// ---------------------------------------------------------------------------
// The table.  Sixteen one-byte entries -- exactly one 128-bit vector register.
// ---------------------------------------------------------------------------

alignas(16) static const uint8_t HEX[16] = {'0','1','2','3','4','5','6','7',
                                            '8','9','a','b','c','d','e','f'};

// Scalar version -- do not modify.  out must have room for 2*n characters.
__attribute__((noinline))
void to_hex(const uint8_t* in, size_t n, char* out) {
  for (size_t i = 0; i < n; i++) {
    out[2 * i]     = HEX[in[i] >> 4];   // high nibble first
    out[2 * i + 1] = HEX[in[i] & 15];   // then low nibble
  }
}

// ---------------------------------------------------------------------------
// Part 3: hand-written intrinsics.  Write the version for YOUR machine.
// ---------------------------------------------------------------------------

#ifdef SIMD_ARCH_X86
// AVX2: 32 input bytes -> 64 characters per iteration.
//
// The intrinsics you need: _mm256_loadu_si256, _mm256_storeu_si256,
// _mm256_broadcastsi128_si256, _mm256_set1_epi8, _mm256_srli_epi16,
// _mm256_and_si256, _mm256_shuffle_epi8, _mm256_unpacklo_epi8,
// _mm256_unpackhi_epi8, _mm256_permute2x128_si256.  Look each one up in the
// Intel intrinsics guide before you use it -- three of them behave in ways
// the name does not tell you.
//
// Finish the leftover 0-31 bytes by calling the scalar to_hex on them.
__attribute__((noinline))
void to_hex_simd(const uint8_t* in, size_t n, char* out) {
  // TODO (Part 3)
  (void)in; (void)n; (void)out;
}
#elif defined(SIMD_ARCH_NEON)
// NEON: 16 input bytes -> 32 characters per iteration.
//
// The intrinsics you need: vld1q_u8, vdupq_n_u8, vshrq_n_u8, vandq_u8,
// vqtbl1q_u8, vst2q_u8 (with the uint8x16x2_t type).  Look each one up in
// Arm's intrinsics reference before you use it.
//
// Finish the leftover 0-15 bytes by calling the scalar to_hex on them.
__attribute__((noinline))
void to_hex_simd(const uint8_t* in, size_t n, char* out) {
  // TODO (Part 3)
  (void)in; (void)n; (void)out;
}
#else
void to_hex_simd(const uint8_t*, size_t, char*) {}  // no SIMD path on this ISA
#endif

// ---------------------------------------------------------------------------
// Part 4: the same algorithm in Highway.
// ---------------------------------------------------------------------------

#ifdef HAVE_HIGHWAY
// Written against a vector of however many bytes the target has: 16 on SSE4
// and NEON, 32 on AVX2, 64 on AVX-512.  Do not use any x86 or NEON intrinsic
// here -- only hn:: operations, so the same source builds on every target.
//
// The operations you need: hn::ScalableTag<uint8_t>, hn::Lanes, hn::LoadU,
// hn::LoadDup128, hn::Set, hn::ShiftRight, hn::And, hn::TableLookupBytes,
// hn::StoreInterleaved2.  Highway's quick_reference.md documents them all.
__attribute__((noinline))
void to_hex_highway(const uint8_t* in, size_t n, char* out) {
  // TODO (Part 4)
  (void)in; (void)n; (void)out;
}
#endif

// ---------------------------------------------------------------------------
// Everything below is the test and timing harness -- do not modify.
// ---------------------------------------------------------------------------

typedef void (*HexFn)(const uint8_t*, size_t, char*);

static const int TRIALS = 7;

// Best time per INPUT byte, in ns.  Repeats the call until each trial has
// encoded about 64 MiB, so small sizes are not lost in timer resolution.
static double time_it(HexFn f, const uint8_t* in, size_t n, char* out) {
  const size_t reps = n >= (64u << 20) ? 1 : (64u << 20) / n;
  double best = 1e300;
  for (int t = 0; t < TRIALS; t++) {
    auto t0 = std::chrono::steady_clock::now();
    for (size_t r = 0; r < reps; r++) {
      asm volatile("" ::"r"(in), "r"(out) : "memory");
      f(in, n, out);
    }
    auto t1 = std::chrono::steady_clock::now();
    double ns = std::chrono::duration<double>(t1 - t0).count() * 1e9 / (double)(reps * n);
    if (ns < best) best = ns;
  }
  return best;
}

// An unmodified stub writes nothing, which this detects before testing.
static bool implemented(HexFn f) {
  const uint8_t one[64] = {0x7a};
  char out[128];
  for (char& c : out) c = 'X';
  f(one, 64, out);
  return out[0] != 'X';
}

// A character as '7', or as \x00 if it is not printable.
static const char* shown(char c, char* buf) {
  if (c >= 0x20 && c < 0x7f) snprintf(buf, 8, "'%c'", c);
  else snprintf(buf, 8, "\\x%02x", (unsigned char)c);
  return buf;
}

// Every length from 0 to 300, from two different starting offsets, against the
// scalar version.  Catches the tail, the interleave order, and writes past 2*n.
static bool agrees(HexFn f, const char* name) {
  std::vector<uint8_t> buf(512);
  for (int k = 0; k < 256; k++) buf[k] = (uint8_t)k;   // every byte value
  std::mt19937 rng(7);
  for (int k = 256; k < 512; k++) buf[k] = (uint8_t)rng();
  for (size_t start : {(size_t)0, (size_t)201}) {
    for (size_t n = 0; n <= 300; n++) {
      std::vector<char> want(2 * n + 8, 'X'), got(2 * n + 8, 'X');
      to_hex(buf.data() + start, n, want.data());
      f(buf.data() + start, n, got.data());
      if (want != got) {
        size_t k = 0;
        while (want[k] == got[k]) k++;
        char w[8], g[8];
        printf("  WRONG: %s, n = %zu, first difference at output character %zu"
               " (expected %s, got %s)\n", name, n, k, shown(want[k], w), shown(got[k], g));
        return false;
      }
    }
  }
  return true;
}

int main() {
  int cpu = pin_to_fast_core();
  double ghz = warm_up();

#if defined(__APPLE__)
  printf("macOS QoS request: %s (not fixed-core pinning)\n",
         cpu < 4 ? "performance" : "background");
#else
  printf("requested cpu%d\n", cpu);
#endif
  printf("%.2f GHz estimated from dependent adds, min of %d trials\n", ghz, TRIALS);
#ifdef HAVE_HIGHWAY
  printf("highway target: %s, %zu byte lanes per vector\n",
         hwy::TargetName(HWY_TARGET), hn::Lanes(hn::ScalableTag<uint8_t>()));
#else
  printf("highway: not found -- install it to do Part 4 (see the handout)\n");
#endif

  // Which versions are written, and are they right?
  struct Version { const char* name; HexFn f; bool ok; };
  std::vector<Version> versions = {{"scalar", to_hex, true}};
#if defined(SIMD_ARCH_X86) || defined(SIMD_ARCH_NEON)
  versions.push_back({"simd", to_hex_simd, false});
#endif
#ifdef HAVE_HIGHWAY
  versions.push_back({"highway", to_hex_highway, false});
#endif
  for (size_t v = 1; v < versions.size(); v++) {
    if (!implemented(versions[v].f))
      printf("%s: not implemented yet\n", versions[v].name);
    else if (agrees(versions[v].f, versions[v].name)) {
      versions[v].ok = true;
      printf("%s: agrees with scalar for n = 0..300 on every byte value\n", versions[v].name);
    }
  }
  printf("\n");

  printf("  %-9s", "input");
  for (auto& v : versions) if (v.ok) printf(" %10s", v.name);
  for (size_t v = 1; v < versions.size(); v++) if (versions[v].ok) printf(" %9s", versions[v].name);
  printf("\n  %-9s", "");
  for (auto& v : versions) if (v.ok) printf(" %10s", "ns/byte");
  for (size_t v = 1; v < versions.size(); v++) if (versions[v].ok) printf(" %9s", "speedup");
  printf("\n");

  struct { size_t n; const char* label; } sizes[] = {
      {1u << 10, "1 KiB"},  {16u << 10, "16 KiB"}, {256u << 10, "256 KiB"},
      {4u << 20, "4 MiB"},  {64u << 20, "64 MiB"},
  };
  std::mt19937 rng(42);
  for (auto s : sizes) {
    std::vector<uint8_t> in(s.n);
    for (auto& b : in) b = (uint8_t)rng();
    std::vector<char> out(2 * s.n);

    std::vector<double> t;
    for (auto& v : versions) t.push_back(v.ok ? time_it(v.f, in.data(), s.n, out.data()) : 0);

    printf("  %-9s", s.label);
    for (size_t v = 0; v < versions.size(); v++) if (versions[v].ok) printf(" %10.4f", t[v]);
    for (size_t v = 1; v < versions.size(); v++) if (versions[v].ok) printf(" %8.1fx", t[0] / t[v]);
    printf("\n");
  }
  return 0;
}
