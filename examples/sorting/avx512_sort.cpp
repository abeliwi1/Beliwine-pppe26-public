// Putting it all together: bitonic networks + an AVX-512 partition on Zen 5.
//
// vqs_highway.cpp replaced libc++'s partition with a CompressStore partition
// and gained only 1.10-1.17x on Apple M5, whose NEON registers hold 4 int32.
// This program asks whether that is a property of the idea or of the width.
// It compiles ONE partition source four times, with Highway's static targets,
// and runs all four on the same core:
//
//   SSE4        4 lanes  table permute + store      (NEON's width)
//   AVX2        8 lanes  table permute + store
//   AVX3       16 lanes  vpcompressd to memory
//   AVX3_ZEN4  16 lanes  vpcompressd to a register, then a plain store
//
// The two 16-lane forms differ only in how they write.  Zen 4 runs the memory
// form of vpcompressd slowly, which is why Highway has a separate Zen target;
// on Zen 5 the two measure the same.
//
// Then it replaces the other scalar piece, the leaf.  Below 24 elements
// libc++'s std::sort runs insertion sort, which mispredicts about once per
// element.  bitonic_leaf() sorts up to 256 elements with a bitonic network
// held in 1-16 AVX-512 registers (16 int32 each), padding the last register
// with INT_MAX.  No comparison in it is a branch.
//
// Everything else follows libc++'s std::sort, exactly as in vqs_highway.cpp:
// its pivot rule, its heapsort depth limit, recurse left and loop right.
//
// Finally it closes the gap to Highway's own vectorized quicksort,
// hwy::VQSort, one design choice at a time (section 6 of the output): an
// in-place partition, unrolled four vectors deep; leaves of up to 256 keys;
// a column-major network layout; a sampled pivot.  Section 7 runs the result
// at all three widths.  Section 8 replaces the bitonic column phase of the
// 256-key network with smaller networks, Batcher's (63 compare-exchanges)
// and Green's (60), against bitonic's 80.
//
// Run only some sections with, e.g., ./avx512_sort 26
//
// Build (Linux, clang, libc++ so that std::sort is the one the M5 numbers used;
// no -march flag: Highway compiles each target with its own attributes):
//   clang++ -O2 -std=c++17 -stdlib=libc++ -I. avx512_sort.cpp -o avx512_sort \
//     -lhwy -lhwy_contrib && ./avx512_sort

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "avx512_sort.cpp"
#include "hwy/foreach_target.h"  // re-includes this file once per target
#include "hwy/highway.h"

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstring>
#include <utility>

#if HWY_TARGET <= HWY_AVX3
  #include <immintrin.h>
#endif

HWY_BEFORE_NAMESPACE();
namespace sorting {
namespace HWY_NAMESPACE {
namespace hn = hwy::HWY_NAMESPACE;

static constexpr int INSERTION_LIMIT   = 24;   // libc++: std::sort below this size
static constexpr int NINTHER_THRESHOLD = 128;  // libc++: median of medians above this

// ================================================================
// simd_partition — vqs_highway.cpp's partition, unchanged.  What it compiles
// to depends on the target: N, the lane count, is 4, 8 or 16.
//
// Partitions a[lo..hi) (hi exclusive) into [ < pivot | >= pivot ] through two
// scratch buffers and returns the number of elements < pivot.
// ================================================================
static int simd_partition(int* a, int lo, int hi, int pivot,
                          int* left_buf, int* right_buf) {
    const hn::ScalableTag<int32_t> d;
    const int N = (int)hn::Lanes(d);

    const auto pivot_v = hn::Set(d, pivot);
    int lc = 0, rc = 0;

    const int vend = lo + ((hi - lo) / N) * N;
    for (int i = lo; i < vend; i += N) {
        const auto v       = hn::LoadU(d, a + i);
        const auto lt_mask = hn::Lt(v, pivot_v);
        lc += (int)hn::CompressStore(v, lt_mask, d, left_buf + lc);
        rc += (int)hn::CompressStore(v, hn::Not(lt_mask), d, right_buf + rc);
    }
    for (int i = vend; i < hi; i++) {
        if (a[i] < pivot) left_buf[lc++] = a[i];
        else              right_buf[rc++] = a[i];
    }

    memcpy(a + lo,      left_buf,  lc * sizeof(int));
    memcpy(a + lo + lc, right_buf, (hi - lo - lc) * sizeof(int));
    return lc;
}

// ================================================================
// inplace_partition — VQSort's partition (Highway's vqsort-inl.h, Partition
// and StoreLeftRight), simplified: the same compare-and-compress per vector as
// simd_partition, but written back into the array itself, with no scratch
// buffers and no copy back.
//
// It first loads U vectors from each end into registers, which leaves a gap
// of U*N free slots at each end.  Then it repeatedly loads U more vectors from
// whichever end has the smaller gap, and writes each one's < pivot lanes at
// writeL (growing right) and its >= pivot lanes just below writeR (growing
// left).  Every load frees as many slots as the writes use, so the writes
// never overtake the reads.  The saved vectors go last.  A scalar loop folds
// in the n % (U*N) leftover elements at the end of the range.
//
// U is the unroll: U = 1 handles one vector per iteration, U = 4 handles four,
// so four compare-compress-store chains can be in flight at once.
//
// How a vector is written depends on the target, as in VQSort:
//   AVX-512: Compress keeps only the selected lanes, so it takes two: a
//            CompressStore of the < pivot lanes and a CompressBlendedStore
//            (a masked store, exactly the >= pivot lanes) below writeR.
//   SSE4, AVX2: Compress is emulated with a table permute that moves the
//            selected lanes to the front and the rest behind them, in order.
//            One Compress, then the same vector is stored twice, whole: once
//            at writeL, once ending at writeR.  The lanes each store gets
//            wrong land in the free gap and are overwritten later.  (These
//            targets have no cheap masked store; SSE4's is a non-temporal
//            maskmovdqu.)
// Unlike VQSort, the choice of end and the leftover loop are branch-free:
// both depend on the data, and a mispredicted branch every 16 elements costs
// about as much as the partition itself.
// ================================================================
template <int U>
static int inplace_partition(int* a, int lo, int hi, int pivot,
                             int* left_buf, int* right_buf) {
    const hn::ScalableTag<int32_t> d;
    using V = hn::Vec<decltype(d)>;
    const int N = (int)hn::Lanes(d), UN = U * N;
    const int n = hi - lo;
    if (n < 2 * UN) return simd_partition(a, lo, hi, pivot, left_buf, right_buf);

    int* b = a + lo;
    const int m = n - n % UN;                  // the vector part: b[0..m)
    const V pivot_v = hn::Set(d, pivot);
    int writeL = 0, writeR = m;

    // `gap_ok` says the free gap between writeL and writeR is at least two
    // vectors wide, which the two whole-vector stores need.  Only the saved
    // vectors, at the very end, can find it narrower.
    auto store_left_right = [&](V v, bool gap_ok) HWY_ATTR {
        const auto lt = hn::Lt(v, pivot_v);
        if constexpr (hn::CompressIsPartition<int32_t>::value) {
            const V lr = hn::Compress(v, lt);         // [ < pivot | >= pivot ]
            const int nl = (int)hn::CountTrue(d, lt);
            if (gap_ok) {
                hn::StoreU(lr, d, b + writeL);         // the < lanes land at writeL
                hn::StoreU(lr, d, b + writeR - N);     // the >= lanes end at writeR
                writeL += nl;
                writeR -= N - nl;
            } else {                                   // gap is exactly one vector
                hn::StoreU(lr, d, b + writeL);
                writeL += nl;
                writeR = writeL;
            }
        } else {
            // The left store may write a whole vector; the right store writes
            // exactly its lanes.  Left goes first, so right overwrites any overshoot.
            const int nl = (int)hn::CompressStore(v, lt, d, b + writeL);
            writeL += nl;
            writeR -= N - nl;
            hn::CompressBlendedStore(v, hn::Not(lt), d, b + writeR);
        }
    };

    V savedL[U], savedR[U];
    for (int u = 0; u < U; u++) {
        savedL[u] = hn::LoadU(d, b + u * N);
        savedR[u] = hn::LoadU(d, b + m - UN + u * N);
    }
    int readL = UN, readR = m - UN;
    while (readL < readR) {
        // Read from the end with the smaller gap: a select, not a branch.
        const bool left = readL - writeL <= writeR - readR;
        const int from = left ? readL : readR - UN;
        readL += left ? UN : 0;
        readR -= left ? 0 : UN;
        V v[U];
        for (int u = 0; u < U; u++) v[u] = hn::LoadU(d, b + from + u * N);
        for (int u = 0; u < U; u++) store_left_right(v[u], true);
    }
    for (int u = 0; u < U; u++) store_left_right(savedL[u], writeR - writeL >= 2 * N);
    for (int u = 0; u < U; u++) store_left_right(savedR[u], writeR - writeL >= 2 * N);

    // b[0..lc) < pivot <= b[lc..i): swap each leftover < pivot to b[lc],
    // without branching on the comparison.
    int lc = writeL;
    for (int i = m; i < n; i++) {
        const int x = b[i], y = b[lc];
        const int lt = x < pivot;
        b[i]  = lt ? y : x;
        b[lc] = lt ? x : y;
        lc += lt;
    }
    return lc;
}

#if HWY_TARGET <= HWY_AVX3
// ================================================================
// bitonic_leaf — sorts n <= 256 elements with a bitonic network held in
// K = 1, 2, 4, 8 or 16 zmm registers.  Element e of the network is lane e % 16
// of register e / 16, which is also its position in memory ("row-major").
//
// One network step compares every element e with its partner e ^ j:
//   j < 16:  the partner is in the same register.  One shuffle brings each
//            lane's partner alongside it, then min, max, and a blend whose
//            mask says which lanes keep the max.
//   j >= 16: the partner is the same lane of register r ^ (j / 16), so the
//            step is a min and a max of two whole registers.
// Within merge stage k, an element sorts its block of k ascending when
// (e & k) == 0 and descending otherwise; the last stage is all ascending.
// ================================================================

// Calls f(R) for R = 0 .. K-1, with R a compile-time constant, so that every
// register index and every blend mask below is a constant and v[] stays in
// registers.  (A plain for loop over r is not unrolled at K = 8, and then
// v[] lives on the stack.)
template <class F, int... R>
HWY_INLINE void for_regs_impl(F&& f, std::integer_sequence<int, R...>) {
    (f(std::integral_constant<int, R>()), ...);
}
template <int K, class F>
HWY_INLINE void for_regs(F&& f) {
    for_regs_impl(f, std::make_integer_sequence<int, K>());
}

// Brings lane l ^ J alongside lane l.
template <int J>
HWY_INLINE __m512i partner(__m512i v) {
    if constexpr (J == 1) return _mm512_shuffle_epi32(v, (_MM_PERM_ENUM)0xB1);  // swap pairs
    if constexpr (J == 2) return _mm512_shuffle_epi32(v, (_MM_PERM_ENUM)0x4E);  // swap pairs of pairs
    if constexpr (J == 4) return _mm512_shuffle_i32x4(v, v, 0xB1);  // swap 128-bit quarters
    if constexpr (J == 8) return _mm512_shuffle_i32x4(v, v, 0x4E);  // swap 256-bit halves
}

// The lanes l for which pred(l) holds, as a mask.
template <class P>
constexpr __mmask16 lanes_where(P pred) {
    unsigned m = 0;
    for (int l = 0; l < 16; l++) if (pred(l)) m |= 1u << l;
    return (__mmask16)m;
}

template <int K, int k, int j>
HWY_INLINE void network_step(__m512i* v) {
    if constexpr (j >= 16) {
        constexpr int jr = j / 16;
        for_regs<K>([&](auto R) HWY_ATTR {
            constexpr int r = R;
            if constexpr ((r & jr) == 0) {
                const __m512i lo = _mm512_min_epi32(v[r], v[r | jr]);
                const __m512i hi = _mm512_max_epi32(v[r], v[r | jr]);
                constexpr bool descending = ((r * 16) & k) != 0;
                v[r]      = descending ? hi : lo;
                v[r | jr] = descending ? lo : hi;
            }
        });
    } else {
        for_regs<K>([&](auto R) HWY_ATTR {
            constexpr int r = R;
            // Lanes that keep the max: the upper element of an ascending
            // pair, or the lower element of a descending one.
            constexpr __mmask16 keep_max = lanes_where([](int l) {
                return ((l & j) != 0) != (((r * 16 + l) & k) != 0); });
            const __m512i p = partner<j>(v[r]);
            v[r] = _mm512_mask_blend_epi32(keep_max, _mm512_min_epi32(v[r], p),
                                           _mm512_max_epi32(v[r], p));
        });
    }
    if constexpr (j > 1) network_step<K, k, j / 2>(v);
}

template <int K, int k>
HWY_INLINE void network_stage(__m512i* v) {
    network_step<K, k, k / 2>(v);
    if constexpr (k < 16 * K) network_stage<K, 2 * k>(v);
}

// Loads n <= 16*K elements into K registers, padding with INT_MAX, which
// sorts to the end and is never stored.
HWY_INLINE __mmask16 tail_mask(int rem) {
    return rem >= 16 ? 0xFFFF : rem <= 0 ? 0 : (__mmask16)((1u << rem) - 1);
}

template <int K>
HWY_INLINE void load_padded(const int* a, int n, __m512i* v) {
    const __m512i pad = _mm512_set1_epi32(INT_MAX);
    for_regs<K>([&](auto R) HWY_ATTR {
        v[R] = _mm512_mask_loadu_epi32(pad, tail_mask(n - 16 * R), a + 16 * R);
    });
}

template <int K>
HWY_INLINE void store_partial(int* a, int n, const __m512i* v) {
    for_regs<K>([&](auto R) HWY_ATTR {
        _mm512_mask_storeu_epi32(a + 16 * R, tail_mask(n - 16 * R), v[R]);
    });
}

template <int K>
HWY_NOINLINE void bitonic_sort_regs(int* a, int n) {
    __m512i v[K];
    load_padded<K>(a, n, v);
    network_stage<K, 2>(v);
    store_partial<K>(a, n, v);
}

// ================================================================
// The column-major network for 256 elements, laid out as VQSort lays out its
// base case: element e is lane e / 16 of register e % 16.  Now it is the
// steps with SMALL j, the most numerous (26 of the 36 steps), whose partner
// is in another register — a min and a max, with no shuffle — and only the
// 10 steps with j >= 16 need a shuffle.  The price is one 16x16 transpose at
// the end, to put element e at position e in memory.  (The input needs no
// transpose: an unsorted input can be loaded in any order.)
// ================================================================
template <int k, int j>
HWY_INLINE void colmajor_step(__m512i* v) {
    if constexpr (j < 16) {                    // partner: same lane of register r ^ j
        for_regs<16>([&](auto R) HWY_ATTR {
            constexpr int r = R;
            if constexpr ((r & j) == 0) {
                const __m512i lo = _mm512_min_epi32(v[r], v[r | j]);
                const __m512i hi = _mm512_max_epi32(v[r], v[r | j]);
                if constexpr (k < 16) {        // direction (e & k) set by the register
                    constexpr bool descending = (r & k) != 0;
                    v[r]     = descending ? hi : lo;
                    v[r | j] = descending ? lo : hi;
                } else {                       // direction set by the lane
                    constexpr __mmask16 desc = lanes_where([](int l) { return (l & (k / 16)) != 0; });
                    v[r]     = _mm512_mask_blend_epi32(desc, lo, hi);
                    v[r | j] = _mm512_mask_blend_epi32(desc, hi, lo);
                }
            }
        });
    } else {                                   // partner: lane l ^ (j / 16), same register
        constexpr __mmask16 keep_max = lanes_where([](int l) {
            return ((l & (j / 16)) != 0) != ((l & (k / 16)) != 0); });
        for_regs<16>([&](auto R) HWY_ATTR {
            const __m512i p = partner<j / 16>(v[R]);
            v[R] = _mm512_mask_blend_epi32(keep_max, _mm512_min_epi32(v[R], p),
                                           _mm512_max_epi32(v[R], p));
        });
    }
    if constexpr (j > 1) colmajor_step<k, j / 2>(v);
}

template <int k>
HWY_INLINE void colmajor_stage(__m512i* v) {
    colmajor_step<k, k / 2>(v);
    if constexpr (k < 256) colmajor_stage<2 * k>(v);
}

// Transposes 16 registers of 16 int32: 64 shuffles.
HWY_INLINE void transpose16(__m512i* v) {
    __m512i t[16], u[16];
    for (int i = 0; i < 8; i++) {              // [a0 b0 a1 b1] [a2 b2 a3 b3] per 128 bits
        t[2 * i]     = _mm512_unpacklo_epi32(v[2 * i], v[2 * i + 1]);
        t[2 * i + 1] = _mm512_unpackhi_epi32(v[2 * i], v[2 * i + 1]);
    }
    for (int i = 0; i < 4; i++) {              // u[4i+c]: column c of each 4x4 block
        u[4 * i]     = _mm512_unpacklo_epi64(t[4 * i],     t[4 * i + 2]);
        u[4 * i + 1] = _mm512_unpackhi_epi64(t[4 * i],     t[4 * i + 2]);
        u[4 * i + 2] = _mm512_unpacklo_epi64(t[4 * i + 1], t[4 * i + 3]);
        u[4 * i + 3] = _mm512_unpackhi_epi64(t[4 * i + 1], t[4 * i + 3]);
    }
    for (int c = 0; c < 4; c++) {              // transpose the 4x4 grid of 128-bit blocks
        const __m512i s0 = _mm512_shuffle_i32x4(u[c],     u[4 + c],  0x44);
        const __m512i s1 = _mm512_shuffle_i32x4(u[c],     u[4 + c],  0xEE);
        const __m512i s2 = _mm512_shuffle_i32x4(u[8 + c], u[12 + c], 0x44);
        const __m512i s3 = _mm512_shuffle_i32x4(u[8 + c], u[12 + c], 0xEE);
        v[c]      = _mm512_shuffle_i32x4(s0, s2, 0x88);
        v[4 + c]  = _mm512_shuffle_i32x4(s0, s2, 0xDD);
        v[8 + c]  = _mm512_shuffle_i32x4(s1, s3, 0x88);
        v[12 + c] = _mm512_shuffle_i32x4(s1, s3, 0xDD);
    }
}

// ================================================================
// Better column networks.  In the column-major layout the first four bitonic
// stages (k = 2 .. 16) sort each column — lane l of the 16 registers — on its
// own, and every one of their compare-exchanges is a min and a max of two
// whole registers.  Any 16-input sorting network can do that job.  Three with
// the same depth, 10, and different sizes:
//
//   bitonic                     80 compare-exchanges
//   Batcher's odd-even merge    63
//   Green's                     60   (the smallest known; VQSort's Sort16)
//
// Batcher's and Green's networks sort every column ascending.  The bitonic
// merge that follows (stage k = 32 on) wants odd columns descending, so the
// odd lanes are then reversed across the registers: 16 blends.
// ================================================================
static constexpr int kBatcher16[63][2] = {
    {0,1}, {2,3}, {0,2}, {1,3}, {1,2}, {4,5}, {6,7}, {4,6}, {5,7}, {5,6}, {0,4},
    {2,6}, {2,4}, {1,5}, {3,7}, {3,5}, {1,2}, {3,4}, {5,6}, {8,9}, {10,11}, {8,10},
    {9,11}, {9,10}, {12,13}, {14,15}, {12,14}, {13,15}, {13,14}, {8,12}, {10,14},
    {10,12}, {9,13}, {11,15}, {11,13}, {9,10}, {11,12}, {13,14}, {0,8}, {4,12},
    {4,8}, {2,10}, {6,14}, {6,10}, {2,4}, {6,8}, {10,12}, {1,9}, {5,13}, {5,9},
    {3,11}, {7,15}, {7,11}, {3,5}, {7,9}, {11,13}, {1,2}, {3,4}, {5,6}, {7,8},
    {9,10}, {11,12}, {13,14}};

// Green's network, as in Highway's sorting_networks-inl.h (Sort16).
static constexpr int kGreen16[60][2] = {
    {0,1}, {2,3}, {4,5}, {6,7}, {8,9}, {10,11}, {12,13}, {14,15}, {0,2}, {1,3},
    {4,6}, {5,7}, {8,10}, {9,11}, {12,14}, {13,15}, {0,4}, {1,5}, {2,6}, {3,7},
    {8,12}, {9,13}, {10,14}, {11,15}, {0,8}, {1,9}, {2,10}, {3,11}, {4,12}, {5,13},
    {6,14}, {7,15}, {5,10}, {6,9}, {3,12}, {7,11}, {13,14}, {4,8}, {1,2}, {1,4},
    {7,13}, {2,8}, {11,14}, {2,4}, {5,6}, {9,10}, {11,13}, {3,8}, {7,12}, {3,5},
    {6,8}, {7,9}, {10,12}, {3,4}, {5,6}, {7,8}, {9,10}, {11,12}, {6,7}, {8,9}};

template <int I, int J>
HWY_INLINE void compare_exchange(__m512i* v) {
    const __m512i lo = _mm512_min_epi32(v[I], v[J]);
    v[J] = _mm512_max_epi32(v[I], v[J]);
    v[I] = lo;
}

template <const auto& NET, size_t... C>
HWY_INLINE void apply_network(__m512i* v, std::index_sequence<C...>) {
    (compare_exchange<NET[C][0], NET[C][1]>(v), ...);
}

enum ColumnNet { BITONIC, BATCHER, GREEN };

template <ColumnNet NET>
HWY_NOINLINE void colmajor_sort256(int* a, int n) {
    __m512i v[16];
    load_padded<16>(a, n, v);
    if constexpr (NET == BITONIC) {
        colmajor_stage<2>(v);
    } else {
        if constexpr (NET == BATCHER)
            apply_network<kBatcher16>(v, std::make_index_sequence<63>());
        else
            apply_network<kGreen16>(v, std::make_index_sequence<60>());
        for (int r = 0; r < 8; r++) {          // reverse the odd columns
            const __m512i x = v[r], y = v[15 - r];
            v[r]      = _mm512_mask_blend_epi32(0xAAAA, x, y);
            v[15 - r] = _mm512_mask_blend_epi32(0xAAAA, y, x);
        }
        colmajor_stage<32>(v);
    }
    transpose16(v);
    store_partial<16>(a, n, v);
}

// Row-major throughout, n <= 256.
static void bitonic_leaf(int* a, int n) {
    if      (n <= 16)  bitonic_sort_regs<1>(a, n);
    else if (n <= 32)  bitonic_sort_regs<2>(a, n);
    else if (n <= 64)  bitonic_sort_regs<4>(a, n);
    else if (n <= 128) bitonic_sort_regs<8>(a, n);
    else               bitonic_sort_regs<16>(a, n);
}

// Row-major up to 128, column-major from 129 to 256.
template <ColumnNet NET = BITONIC>
static void bitonic_leaf_cm(int* a, int n) {
    if (n <= 128) bitonic_leaf(a, n);
    else          colmajor_sort256<NET>(a, n);
}
#endif  // HWY_TARGET <= HWY_AVX3

// ================================================================
// Pivot selection, as libc++ does it (see vqs_highway.cpp).
// ================================================================
static inline void sort3(int* a, int i, int j, int k) {
    if (a[j] < a[i]) std::swap(a[i], a[j]);
    if (a[k] < a[j]) {
        std::swap(a[j], a[k]);
        if (a[j] < a[i]) std::swap(a[i], a[j]);
    }
}

static inline void choose_pivot(int* a, int lo, int hi) {
    const int len = hi - lo + 1, half = len / 2;
    if (len > NINTHER_THRESHOLD) {
        sort3(a, lo,     lo + half,     hi);
        sort3(a, lo + 1, lo + half - 1, hi - 1);
        sort3(a, lo + 2, lo + half + 1, hi - 2);
        sort3(a, lo + half - 1, lo + half, lo + half + 1);
        std::swap(a[lo], a[lo + half]);
    } else {
        sort3(a, lo + half, lo, hi);
    }
}

// ================================================================
// A sampled pivot, after VQSort's (DrawSamples, SortSamples,
// ChoosePivotByRank): 32 samples, each the median of 3 keys, sorted, and the
// middle one taken.  VQSort draws its keys from random cache lines; these are
// evenly spaced, which is as good on random input but could be defeated by
// an adversary.  The chosen key is swapped to a[lo], where the driver expects
// the pivot.
// ================================================================
static constexpr int SAMPLES = 32;

using LeafFn = void (*)(int*, int);

static void sampled_pivot(int* a, int lo, int hi, LeafFn leaf) {
    const int len = hi - lo + 1, stride = len / (3 * SAMPLES);
    int med[SAMPLES];
    for (int i = 0; i < SAMPLES; i++) {         // median of 3 with min and max
        const int x = a[lo + (3 * i) * stride];
        const int y = a[lo + (3 * i + 1) * stride];
        const int z = a[lo + (3 * i + 2) * stride];
        med[i] = std::max(std::min(x, y), std::min(std::max(x, y), z));
    }
    leaf(med, SAMPLES);
    const int pivot = med[SAMPLES / 2];
    for (int t = 0; t < 3 * SAMPLES; t++)       // find a key equal to it
        if (a[lo + t * stride] == pivot) { std::swap(a[lo], a[lo + t * stride]); return; }
}

// ================================================================
// The driver, shaped like libc++'s __introsort.  Sorts a[lo..hi] inclusive.
//
// LEAF == 0: partitions below 24 go to std::sort (libc++'s insertion sort).
// LEAF  > 0: partitions of at most LEAF elements go to leaf(), an AVX-512
//            bitonic network.  Every target calls the same one through a
//            pointer, so the 4- and 8-lane sorts differ from the 16-lane
//            sort in their partition only.
// PART == 0: simd_partition, through scratch buffers.
// PART == U: inplace_partition, unrolled U vectors deep.
// SAMPLE:    the sampled pivot above 3*SAMPLES elements, else libc++'s rule.
// ================================================================
template <int LEAF, int PART, bool SAMPLE>
static void qs_core(int* a, int lo, int hi, int depth, int* left_buf, int* right_buf,
                    LeafFn leaf) {
    while (true) {
        const int len = hi - lo + 1;
        if constexpr (LEAF == 0) {
            if (len < INSERTION_LIMIT) { std::sort(a + lo, a + hi + 1); return; }
        } else {
            if (len <= LEAF) { if (len > 1) leaf(a + lo, len); return; }
        }
        if (depth == 0) {
            std::partial_sort(a + lo, a + hi + 1, a + hi + 1);
            return;
        }
        --depth;

        if (SAMPLE && len >= 3 * SAMPLES) sampled_pivot(a, lo, hi, leaf);
        else                              choose_pivot(a, lo, hi);
        const int pivot = a[lo];
        const int lc = PART == 0
            ? simd_partition(a, lo + 1, hi + 1, pivot, left_buf, right_buf)
            : inplace_partition<PART == 0 ? 1 : PART>(a, lo + 1, hi + 1, pivot,
                                                      left_buf, right_buf);
        const int p = lo + lc;
        std::swap(a[lo], a[p]);

        qs_core<LEAF, PART, SAMPLE>(a, lo, p - 1, depth, left_buf, right_buf, leaf);
        lo = p + 1;
    }
}

// Scratch buffers are allocated by the caller, once, and reused across sorts.
template <int LEAF, int PART = 0, bool SAMPLE = false>
void Sort(int* a, int n, int* left_buf, int* right_buf, LeafFn leaf) {
    if (n < 2) return;
    qs_core<LEAF, PART, SAMPLE>(a, 0, n - 1, 2 * (int)std::log2((double)n),
                                left_buf, right_buf, leaf);
}

// One partition pass over all of a[0..n) around pivot, for timing the
// partition by itself.
template <int PART>
int Partition(int* a, int n, int pivot, int* left_buf, int* right_buf) {
    if constexpr (PART == 0) return simd_partition(a, 0, n, pivot, left_buf, right_buf);
    else return inplace_partition<PART>(a, 0, n, pivot, left_buf, right_buf);
}

int Lanes() { return (int)hn::Lanes(hn::ScalableTag<int32_t>()); }

#if HWY_TARGET <= HWY_AVX3
void Leaf(int* a, int n) { bitonic_leaf(a, n); }
void LeafCM(int* a, int n) { bitonic_leaf_cm(a, n); }
void LeafBatcher(int* a, int n) { bitonic_leaf_cm<BATCHER>(a, n); }
void LeafGreen(int* a, int n) { bitonic_leaf_cm<GREEN>(a, n); }
#endif

}  // namespace HWY_NAMESPACE
}  // namespace sorting
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
// ======================================================================
// Everything below is compiled once, for the baseline target.
// ======================================================================
#include "hwy/contrib/sort/vqsort.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#if defined(__linux__)
  #include <sched.h>
#endif

#if !(HWY_TARGETS & HWY_SSE4) || !(HWY_TARGETS & HWY_AVX2) || \
    !(HWY_TARGETS & HWY_AVX3) || !(HWY_TARGETS & HWY_AVX3_ZEN4)
  #error "this compiler does not build all four Highway targets"
#endif

using namespace std;
using namespace std::chrono;

static constexpr int RUNS = 7;

// ---------------------------------------------------------------------------
// Measurement harness — the one in vqs_highway.cpp, for Linux x86.  Pin to a
// full Zen 5 core (cpu0-3 on a Ryzen AI 9 HX 370; cpu4-11 are the smaller
// Zen 5c cores), raise the clock with a high-IPC warm-up and report it, and
// give every sort a different random input so the branch predictor cannot
// learn the data.
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
#endif
    return cpu;
}

// A dependent chain of integer adds retires one per cycle, so this reports the
// clock the core is running at now rather than its nameplate maximum.
static double core_clock_ghz() {
    const long n = 100000000;
    long a = 0;
    auto t0 = steady_clock::now();
    for (long i = 0; i < n / 8; i++)
        asm volatile("addq $1,%0\n\taddq $1,%0\n\taddq $1,%0\n\taddq $1,%0\n\t"
                     "addq $1,%0\n\taddq $1,%0\n\taddq $1,%0\n\taddq $1,%0"
                     : "+r"(a));
    auto t1 = steady_clock::now();
    return n / duration<double>(t1 - t0).count() / 1e9;
}

// Eight INDEPENDENT chains, so this is high-IPC and the governor responds to it.
static double warm_up(double seconds = 0.8) {
    long a0 = 0, a1 = 0, a2 = 0, a3 = 0, a4 = 0, a5 = 0, a6 = 0, a7 = 0;
    auto t0 = steady_clock::now();
    while (duration<double>(steady_clock::now() - t0).count() < seconds)
        for (int i = 0; i < 200000; i++)
            asm volatile("addq $1,%0\n\taddq $1,%1\n\taddq $1,%2\n\taddq $1,%3\n\t"
                         "addq $1,%4\n\taddq $1,%5\n\taddq $1,%6\n\taddq $1,%7"
                         : "+r"(a0),"+r"(a1),"+r"(a2),"+r"(a3),"+r"(a4),"+r"(a5),"+r"(a6),"+r"(a7));
    return core_clock_ghz();
}

// Best time per sort of n elements, in ns, over RUNS trials.  Each trial sorts
// about 100,000 elements in total (at least 4 sorts), every sort a different
// random input from a pool of up to 4096 arrays.  The pool is the same for
// every sort function at a given n, so rows compare like with like.
template <typename SortFn>
static double bench_sort_ns(int n, SortFn sort_fn) {
    const int reps  = max(4, 100000 / n);
    const int count = min(reps, 4096);
    mt19937 rng(42);
    uniform_int_distribution<int> dist(INT_MIN, INT_MAX);
    vector<int> pool((size_t)count * n);
    for (int& x : pool) x = dist(rng);

    vector<int> work(n);
    double best = 1e300;
    for (int r = 0; r < RUNS; r++) {
        auto t0 = steady_clock::now();
        for (int k = 0; k < reps; k++) {
            const int* src = pool.data() + (size_t)(k % count) * n;
            copy(src, src + n, work.begin());
            sort_fn(work.data(), n);
            asm volatile("" ::"r"(work.data()) : "memory");
        }
        auto t1 = steady_clock::now();
        best = min(best, duration<double>(t1 - t0).count() * 1e9 / reps);
    }
    return best;
}

// Three significant figures: 8.52 ns, 63.8 ns, 137 ns, 1.61 ms.
static string fmt_time(double ns) {
    const char* unit = "ns";
    if (ns >= 1e9)      { ns /= 1e9; unit = "s";  }
    else if (ns >= 1e6) { ns /= 1e6; unit = "ms"; }
    else if (ns >= 1e3) { ns /= 1e3; unit = "µs"; }
    char buf[32];
    snprintf(buf, sizeof buf, "%.*f %s", ns < 10 ? 2 : ns < 100 ? 1 : 0, ns, unit);
    return buf;
}

static string fmt_x(double x) {
    char buf[16];
    snprintf(buf, sizeof buf, "%.2fx", x);
    return buf;
}

// libc++'s introsort compiled from the header with its branch-free bitset
// partition forced on — the std::sort that the Apple M5 numbers measured.
// Ubuntu's precompiled std::sort<int> does not take the bitset partition (it
// runs no faster than libstdc++'s), so on this machine plain std::sort is not
// the same baseline.  See sort_compare_large.cpp for the same call.
static void libcxx_sort(int* a, int n) {
    std::__less<> comp;
    std::__introsort<std::_ClassicAlgPolicy, std::__less<>&, int*, true>(
        a, a + n, comp, n < 2 ? 0 : 2 * std::__log2i(n));
}

// The sorts, as plain functions of (int*, n).  The scratch buffers are sized
// for the largest n and shared.
static constexpr int MAX_N = 1 << 22;
static vector<int> g_left(MAX_N), g_right(MAX_N);

#define VQS(TARGET, LEAF) [](int* a, int n) {                        \
    sorting::N_##TARGET::Sort<LEAF>(a, n, g_left.data(), g_right.data(), \
                                    sorting::N_AVX3_ZEN4::Leaf); }

// The full set of switches: leaf size, partition, sampled pivot, leaf layout.
#define VQS_X(TARGET, LEAF, PART, SAMPLE, LEAFFN) [](int* a, int n) {          \
    sorting::N_##TARGET::Sort<LEAF, PART, SAMPLE>(a, n, g_left.data(),          \
                                                  g_right.data(), LEAFFN); }

struct Row { const char* name; void (*fn)(int*, int); };

static const Row kSmall[] = {
    {"std::sort",  libcxx_sort},
    {"bitonic",    sorting::N_AVX3_ZEN4::Leaf},
};

// The two layouts of the bitonic network, on one leaf of n elements, and
// VQSort, which at n <= 256 is its base case alone.
static const Row kLayout[] = {
    {"row-major",  sorting::N_AVX3_ZEN4::Leaf},
    {"col-major",  sorting::N_AVX3_ZEN4::LeafCM},
    {"VQSort",     [](int* a, int n) { hwy::VQSort(a, (size_t)n, hwy::SortAscending()); }},
};

// The M5 experiment, repeated at three widths: only the partition differs
// from std::sort.
static const Row kLaneSweep[] = {
    {"std::sort",  libcxx_sort},
    {"4 lanes",    VQS(SSE4, 0)},
    {"8 lanes",    VQS(AVX2, 0)},
    {"16 lanes",   VQS(AVX3_ZEN4, 0)},
};

static const Row kLeafSweep[] = {
    {"insert<24",  VQS(AVX3_ZEN4, 0)},
    {"bitonic16",  VQS(AVX3_ZEN4, 16)},
    {"bitonic32",  VQS(AVX3_ZEN4, 32)},
    {"bitonic64",  VQS(AVX3_ZEN4, 64)},
    {"bitonic128", VQS(AVX3_ZEN4, 128)},
};

// Both scalar pieces replaced: SIMD partition at each width, bitonic leaves.
static const Row kAllTogether[] = {
    {"std::sort",  libcxx_sort},
    {"4 lanes",    VQS(SSE4, 64)},
    {"8 lanes",    VQS(AVX2, 64)},
    {"16 lanes",   VQS(AVX3_ZEN4, 64)},
    {"VQSort",     [](int* a, int n) { hwy::VQSort(a, (size_t)n, hwy::SortAscending()); }},
};

// From our sort to VQSort, one design choice at a time, all at 16 lanes.
// Each row's speedup is over the row above it.
#define LEAF_RM sorting::N_AVX3_ZEN4::Leaf
#define LEAF_CM sorting::N_AVX3_ZEN4::LeafCM
static const Row kLadder[] = {
    {"ours",        VQS_X(AVX3_ZEN4,  64, 0, false, LEAF_RM)},
    {"in place",    VQS_X(AVX3_ZEN4,  64, 1, false, LEAF_RM)},
    {"unroll 4",    VQS_X(AVX3_ZEN4,  64, 4, false, LEAF_RM)},
    {"leaf 256",    VQS_X(AVX3_ZEN4, 256, 4, false, LEAF_RM)},
    {"col-major",   VQS_X(AVX3_ZEN4, 256, 4, false, LEAF_CM)},
    {"sampled",     VQS_X(AVX3_ZEN4, 256, 4, true,  LEAF_CM)},
    {"VQSort",      [](int* a, int n) { hwy::VQSort(a, (size_t)n, hwy::SortAscending()); }},
};

// Every one of VQSort's choices, at each width.
static const Row kBestByWidth[] = {
    {"std::sort",  libcxx_sort},
    {"4 lanes",    VQS_X(SSE4,      256, 4, true, LEAF_CM)},
    {"8 lanes",    VQS_X(AVX2,      256, 4, true, LEAF_CM)},
    {"16 lanes",   VQS_X(AVX3_ZEN4, 256, 4, true, LEAF_CM)},
    {"VQSort",     [](int* a, int n) { hwy::VQSort(a, (size_t)n, hwy::SortAscending()); }},
};

// The column phase of the 256-key network: bitonic, Batcher, Green.
static const Row kColumnNets[] = {
    {"bitonic",    sorting::N_AVX3_ZEN4::LeafCM},
    {"Batcher",    sorting::N_AVX3_ZEN4::LeafBatcher},
    {"Green",      sorting::N_AVX3_ZEN4::LeafGreen},
    {"VQSort",     [](int* a, int n) { hwy::VQSort(a, (size_t)n, hwy::SortAscending()); }},
};

// The best sort of section 7 (16 lanes) with each column network.
static const Row kColumnNetSorts[] = {
    {"bitonic",    VQS_X(AVX3_ZEN4, 256, 4, true, sorting::N_AVX3_ZEN4::LeafCM)},
    {"Batcher",    VQS_X(AVX3_ZEN4, 256, 4, true, sorting::N_AVX3_ZEN4::LeafBatcher)},
    {"Green",      VQS_X(AVX3_ZEN4, 256, 4, true, sorting::N_AVX3_ZEN4::LeafGreen)},
    {"VQSort",     [](int* a, int n) { hwy::VQSort(a, (size_t)n, hwy::SortAscending()); }},
};

// One partition pass over the whole array around pivot 0, the median of the
// random inputs, so the split is 50/50 and as unpredictable as it gets.
#define PART(TARGET, P) [](int* a, int n) { \
    a[0] = 0; sorting::N_##TARGET::Partition<P>(a, n, 0, g_left.data(), g_right.data()); }

static const Row kPartition[] = {
    {"copy only",  [](int* a, int) { a[0] = 0; }},
    {"bitset",     [](int* a, int n) {
                       a[0] = 0;   // the pivot, which libc++ expects first
                       std::__less<> comp;
                       std::__bitset_partition<std::_ClassicAlgPolicy, int*, std::__less<>&>(
                           a, a + n, comp); }},
    {"4 buf",      PART(SSE4, 0)},
    {"8 buf",      PART(AVX2, 0)},
    {"16 mem",     PART(AVX3, 0)},
    {"16 buf",     PART(AVX3_ZEN4, 0)},
    {"4 inpl x4",  PART(SSE4, 4)},
    {"8 inpl x4",  PART(AVX2, 4)},
    {"16 inpl",    PART(AVX3_ZEN4, 1)},
    {"16 inpl x4", PART(AVX3_ZEN4, 4)},
};

static bool check(void (*fn)(int*, int), int n, mt19937& rng, int kind) {
    uniform_int_distribution<int> dist(INT_MIN, INT_MAX);
    vector<int> v(n);
    for (int i = 0; i < n; i++)
        v[i] = kind == 0 ? dist(rng) : kind == 1 ? i : kind == 2 ? 7 : (int)(dist(rng) % 5);
    if (kind == 0 && n > 0) v[0] = INT_MAX;   // a real INT_MAX among the padding
    vector<int> ref = v;
    sort(ref.begin(), ref.end());
    fn(v.data(), n);
    return v == ref;
}

// One table: a column per entry in `rows`, then each one's speedup over the
// first.
template <size_t R>
static void table(const char* title, const Row (&rows)[R], const vector<int>& ns) {
    const int W = 10 + 11 * (int)R + 9 * ((int)R - 1);
    cout << "\n" << title << "\n" << string(W, '-') << "\n" << setw(10) << "n";
    for (const Row& r : rows) cout << setw(11) << r.name;
    for (size_t i = 1; i < R; i++) cout << setw(9) << "speedup";
    cout << "\n" << string(W, '-') << "\n";
    for (int n : ns) {
        double t[R];
        for (size_t i = 0; i < R; i++) t[i] = bench_sort_ns(n, rows[i].fn);
        cout << setw(10) << n;
        for (size_t i = 0; i < R; i++) cout << setw(11) << fmt_time(t[i]);
        for (size_t i = 1; i < R; i++) cout << setw(9) << fmt_x(t[0] / t[i]);
        cout << "\n";
    }
    cout << string(W, '-') << "\n";
}

// Rows are steps; each cell is the time at one n and its speedup over the
// step above.
template <size_t R>
static void ladder(const char* title, const Row (&rows)[R], const vector<int>& ns) {
    const int W = 12 + 21 * (int)ns.size();
    cout << "\n" << title << "\n" << string(W, '-') << "\n" << left << setw(12) << "" << right;
    for (int n : ns) cout << setw(12) << ("n=" + to_string(n)) << setw(9) << "step";
    cout << "\n" << string(W, '-') << "\n";
    vector<vector<double>> t(R, vector<double>(ns.size()));
    for (size_t i = 0; i < R; i++)
        for (size_t c = 0; c < ns.size(); c++) t[i][c] = bench_sort_ns(ns[c], rows[i].fn);
    for (size_t i = 0; i < R; i++) {
        cout << left << setw(12) << rows[i].name << right;
        for (size_t c = 0; c < ns.size(); c++)
            cout << setw(12) << fmt_time(t[i][c])
                 << setw(9) << (i ? fmt_x(t[i - 1][c] / t[i][c]) : string(""));
        cout << "\n";
    }
    cout << string(W, '-') << "\n";
}

// Partitions a copy of v around 0 and checks that the result is a
// permutation of v, split at the returned count.
template <class PartFn>
static bool check_partition(PartFn part, vector<int> v) {
    vector<int> w = v;
    const int lc = part(w.data(), (int)w.size());
    for (int i = 0; i < (int)w.size(); i++)
        if ((i < lc) != (w[i] < 0)) return false;
    sort(v.begin(), v.end());
    sort(w.begin(), w.end());
    return v == w;
}

// Run only some sections: ./avx512_sort 26 runs sections 2 and 6.
int main(int argc, char** argv) {
    auto want = [&](char c) { return argc < 2 || strchr(argv[1], c) != nullptr; };
    const int cpu = pin_to_fast_core();
    const double ghz = warm_up();
    printf("cpu%d, %.2f GHz after warm-up (measured), best of %d trials,"
           " a different random input for every sort\n", cpu, ghz, RUNS);
    printf("lanes: SSE4 %d, AVX2 %d, AVX3 %d, AVX3_ZEN4 %d\n",
           sorting::N_SSE4::Lanes(), sorting::N_AVX2::Lanes(),
           sorting::N_AVX3::Lanes(), sorting::N_AVX3_ZEN4::Lanes());

    const int64_t need = HWY_SSE4 | HWY_AVX2 | HWY_AVX3 | HWY_AVX3_ZEN4;
    if ((hwy::SupportedTargets() & need) != need) {
        fprintf(stderr, "this CPU does not support all four targets (needs AVX-512 on Zen 4 or later)\n");
        return 1;
    }

    // --- Correctness: random (with an INT_MAX), sorted, all-equal, 5 distinct values ---
    {
        mt19937 rng(99);
        bool ok = true;
        const int sizes[] = {0, 1, 2, 15, 16, 17, 23, 24, 31, 33, 64, 65, 100, 127,
                             128, 129, 255, 256, 257, 300, 1000, 4097, 100000, 1000003};
        vector<Row> all(begin(kLaneSweep), end(kLaneSweep));
        all.insert(all.end(), begin(kLeafSweep), end(kLeafSweep));
        all.insert(all.end(), begin(kAllTogether), end(kAllTogether));
        all.insert(all.end(), begin(kLadder), end(kLadder));
        all.insert(all.end(), begin(kBestByWidth), end(kBestByWidth));
        all.insert(all.end(), begin(kColumnNetSorts), end(kColumnNetSorts));
        for (const Row& row : all)
            for (int n : sizes)
                for (int kind = 0; kind < 4; kind++)
                    if (!check(row.fn, n, rng, kind)) {
                        printf("FAIL: %s, n=%d, input kind %d\n", row.name, n, kind);
                        ok = false;
                    }
        for (int n = 0; n <= 256; n++)
            for (const Row& row : {kLayout[0], kLayout[1], kColumnNets[1], kColumnNets[2]})
                if (!check(row.fn, n, rng, n % 4)) {
                    printf("FAIL: %s leaf, n=%d\n", row.name, n);
                    ok = false;
                }
        uniform_int_distribution<int> dist(INT_MIN, INT_MAX);
        for (int n : {0, 1, 100, 127, 128, 129, 1000, 4096, 4097, 100003}) {
            vector<int> v(n);
            for (int& x : v) x = dist(rng);
            const auto l = g_left.data(), r = g_right.data();
            const bool good =
                check_partition([&](int* a, int m) { return sorting::N_SSE4::Partition<4>(a, m, 0, l, r); }, v) &&
                check_partition([&](int* a, int m) { return sorting::N_AVX2::Partition<4>(a, m, 0, l, r); }, v) &&
                check_partition([&](int* a, int m) { return sorting::N_AVX3_ZEN4::Partition<1>(a, m, 0, l, r); }, v) &&
                check_partition([&](int* a, int m) { return sorting::N_AVX3_ZEN4::Partition<4>(a, m, 0, l, r); }, v) &&
                check_partition([&](int* a, int m) { return sorting::N_AVX3::Partition<0>(a, m, 0, l, r); }, v);
            if (!good) { printf("FAIL: partition, n=%d\n", n); ok = false; }
        }
        printf("Correctness: %s\n", ok ? "PASS" : "FAIL");
        if (!ok) return 1;
    }

    const vector<int> sizes = {32, 128, 1024, 8192, 65536, 131072, 1 << 20, 1 << 22};

    // --- 1. The leaf by itself ---
    if (want('1')) {
    table("1. One small sort: std::sort vs a bitonic network in zmm registers",
          kSmall, {16, 32, 64, 128});
    printf("cycles at n=16: std::sort %.0f, bitonic %.0f\n",
           bench_sort_ns(16, kSmall[0].fn) * ghz, bench_sort_ns(16, kSmall[1].fn) * ghz);
    table("1b. The two network layouts, one leaf (the same code up to 128), and VQSort",
          kLayout, {32, 64, 128, 160, 256});
    }

    // --- 2. One partition pass, by itself, in cycles per element ---
    if (want('2')) {
        const int R = sizeof kPartition / sizeof kPartition[0];
        cout << "\n2. One partition pass, cycles per element (copying the input subtracted)\n";
        cout << string(10 + 11 * (R - 1), '-') << "\n" << setw(10) << "n";
        for (int i = 1; i < R; i++) cout << setw(11) << kPartition[i].name;
        cout << "\n" << string(10 + 11 * (R - 1), '-') << "\n";
        for (int n : {1024, 65536, 1 << 20}) {
            const double copy_ns = bench_sort_ns(n, kPartition[0].fn);
            cout << setw(10) << n;
            for (int i = 1; i < R; i++)
                cout << setw(11) << fixed << setprecision(2)
                     << (bench_sort_ns(n, kPartition[i].fn) - copy_ns) * ghz / n;
            cout << "\n";
        }
        cout << string(10 + 11 * (R - 1), '-') << "\n";
    }

    // --- 3. The M5 experiment at three widths: only the partition differs ---
    if (want('3')) table("3. The partition at 4, 8 and 16 lanes; everything else is std::sort's",
          kLaneSweep, sizes);

    // --- 4. The leaf, with the 16-lane partition ---
    if (want('4')) table("4. The leaf: insertion sort below 24, or a bitonic network up to 16-128",
          kLeafSweep, {1024, 65536, 1 << 20});

    // --- 5. All together ---
    if (want('5')) table("5. All together: SIMD partition + bitonic leaves (<= 64), and hwy::VQSort",
          kAllTogether, sizes);

    // --- 6. From our sort to VQSort, one change at a time ---
    if (want('6'))
        ladder("6. From our sort to VQSort, one change at a time (16 lanes)",
               kLadder, {1024, 65536, 1 << 20});

    // --- 7. Every VQSort choice, at each width ---
    if (want('7'))
        table("7. With every VQSort choice: in place x4, 256-key leaves, sampled pivot",
              kBestByWidth, sizes);

    // --- 8. Better column networks than bitonic ---
    if (want('8')) {
        table("8. The 256-key leaf's column phase: bitonic (80), Batcher (63), Green (60)",
              kColumnNets, {160, 200, 256});
        table("8b. The same, in the whole sort (section 7's 16-lane sort)",
              kColumnNetSorts, {1024, 65536, 1 << 20});
    }

    printf("\nfinal clock check: %.2f GHz\n", core_clock_ghz());
    return 0;
}
#endif  // HWY_ONCE
