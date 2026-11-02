# Activity 3 — Vectorizing a Table Lookup Written Responses

## Machine and build

- CPU model: Apple M4 (MacBook Pro 10-core - 4 performance cores and 6 efficiency cores; 16 GB)
- Operating system: MacOS 27.0.1
- Compiler and version: Apple clang 21.0.0 (clang-2100.3.34.2), target arm64-apple-darwin27.0.0
- Build flags: -O3 -std=c++17 -I/opt/homebrew/include -L/opt/homebrew/lib -lhwy
- Highway target: NEON, 16 byte lanes per vector

## 3a — Part 1: Ask the compiler

### (a)

Output:

```
activity3_hex.cpp:124:3: remark: the cost-model indicates that vectorization is not
      beneficial [-Rpass-missed=loop-vectorize]
  124 |   for (size_t i = 0; i < n; i++) {
      |   ^    
activity3_hex.cpp:124:3: remark: the cost-model indicates that interleaving is not beneficial
      [-Rpass-missed=loop-vectorize]
```
Reports as not vectorized

Both lines are `-Rpass-missed=loop-vectorize`, so clang did not report the loop as vectorized. The cost model decided that vectorizing it would not pay off, and that interleaving would not either.

### (b)

`__Z6to_hexPKhmPc` contains no `tbl` instructions. The whole file has none. Each nibble is looked up with a scalar indexed byte load from `HEX` (`x9` holds the table address):

```assembly
ldrb    w10, [x0]          ; input byte
lsr     x10, x10, #4       ; high nibble
ldrb    w10, [x9, x10]     ; HEX[high nibble]
sturb   w10, [x8, #-1]     ; store high character
ldrb    w10, [x0], #1      ; same input byte, then advance
and     x10, x10, #0xf     ; low nibble
ldrb    w10, [x9, x10]     ; HEX[low nibble]
strb    w10, [x8], #2      ; store low character
```

The two `ldrb w10, [x9, x10]` instructions are the hex table lookups: one for the high nibble, one for the low nibble, once per input byte. The shift and the mask only build the index. Nothing in the loop is a vector shuffle.


### (c)

No. The cost model declined the loop, and the assembly is still one input byte per iteration. Each nibble is turned into an index and then loaded from `HEX` in memory with `ldrb`; clang never put the 16-byte table in a vector register or replaced those loads with `tbl`. The lookups stayed two dependent scalar memory accesses per input byte, the same work as the source loop.


## 3b — Part 2: Predict

| Quantity | Value |
|---|---|
| Vector register width (bits) | 128 |
| Bytes per vector register | 16 |
| Input bytes per loop iteration | 16 |
| Ideal speedup from lane count alone | 16× |
| Your predicted speedup at 16 KiB | 18× |

### (d)

The lane-count estimate is 16×: one NEON register holds 16 input bytes (indices), and the scalar loop handles only one.

Per input byte the scalar assembly does 4 loads and 2 stores: the input byte twice, `HEX` twice, and one store for each output character. With the shift, the mask, the decrement, and the branch, that is about 10 instructions per byte, or about 160 instructions per 16 bytes.

The NEON loop should do about 9 instructions for those same 16 bytes: one `vld1q_u8`, a shift, a mask, two `vqtbl1q_u8` lookups, one `vst2q_u8` of the 32 output characters, and a few instructions of pointer and branch overhead. The table load and the `0x0f` mask can sit outside the loop. That ratio is 160 / 9 ≈ 18×.

I predict 18×, a little above the lane count. Two of the four scalar loads are `HEX` lookups, and those become register shuffles rather than memory loads. The scalar branch also runs once per byte instead of once per 16 bytes. The extra vector instructions (split and interleave) are real, but they are a handful against 16 bytes of scalar work. At 16 KiB the input plus 32 KiB of output fits in my 128 KB L1 cache, so I do not expect memory bandwidth to limit/hold this case down.


## 3c — Part 3: `to_hex_simd`

### (e)

Each input byte is split inside its own NEON lane. `vshrq_n_u8(bytes, 4)` shifts that byte right by 4, and a NEON byte shift fills the vacated bits with zeros, so the lane is already the high nibble, in 0–15 (with 4 remaining binary bits). No mask is needed after the shift. The low nibble is the original byte with its top four bits cleared, `vandq_u8(bytes, vdupq_n_u8(0x0f))`. The mask belongs on that original byte: an index of 16 or more is outside the 16-entry table, and `vqtbl1q_u8` would return 0 for it.

### (f)

The two lookups stay in separate registers. `chars.val[0]` is the 16 high-nibble characters and `chars.val[1]` is the 16 low-nibble characters, still grouped by nibble rather than paired per input byte. `vst2q_u8` stores those two registers as 16 pairs: lane i of `val[0]`, then lane i of `val[1]`. That writes high character, low character, for each input byte, 32 bytes in one store.

Without it, the registers have to be interleaved first and then stored in two halves. `vzipq_u8` puts the first eight pairs in one register and the last eight in the other:

```cpp
uint8x16x2_t zipped = vzipq_u8(chars.val[0], chars.val[1]);
vst1q_u8(reinterpret_cast<uint8_t*>(out + 2 * i), zipped.val[0]);
vst1q_u8(reinterpret_cast<uint8_t*>(out + 2 * i + 16), zipped.val[1]);
```


## 3d — Part 4: `to_hex_highway`

### (g)

The NEON loop body is 7 lines. The Highway loop body is 6 statements, 7 source lines only because `StoreInterleaved2` wraps. Highway is shorter by the `uint8x16x2_t` and the two assignments into `val[0]` and `val[1]`.

The speeds pretty much match. At 16 KiB the NEON version took 0.0289 ns/byte (12.6×) and Highway took 0.0283 ns/byte (12.9×). Across the other sizes they also stay very close within a few percent of each other.

`hn::StoreInterleaved2` writes lane i of the high-character vector and then lane i of the low-character vector, so each input byte becomes its two hex characters in order. That is steps 4 and 5 of Part 3, the interleave and the store, which `vst2q_u8` did as one instruction.


## 3e — Part 5: Measure

  input         scalar       simd    highway      simd   highway
               ns/byte    ns/byte    ns/byte   speedup   speedup
  1 KiB         0.3655     0.0311     0.0311     11.7x     11.8x
  16 KiB        0.3636     0.0289     0.0283     12.6x     12.9x
  256 KiB       0.3666     0.0297     0.0306     12.3x     12.0x
  4 MiB         0.3684     0.0302     0.0303     12.2x     12.2x
  64 MiB        0.3754     0.0336     0.0338     11.2x     11.1x

### (h)

The prediction was 18×. At 16 KiB the measured speedups are 12.6× for the NEON loop and 12.9× for Highway, so the prediction is about 5× high. Both are also below the 16× lane-count estimate, which is the opposite of what Part 2 expected.

The instruction ratio treated 10 scalar instructions and about 9 vector instructions as if they cost the same. They do not. The scalar loop is about 0.36 ns/byte, roughly 1.6 cycles per byte on this core, so several of those loads, the shift, the mask, and the branch are already overlapped. Counting them as 160 serial instructions overstated how much time the vector loop had to beat. The `HEX` loads that the shuffle removes were L1 hits, not misses, so turning them into `tbl` was a smaller win than the prediction assumed.

What is left in the vector loop is a dependent chain: load, split, two table lookups, then a store of 32 bytes. That is more than one instruction per lane, which is why the speedup lands below 16× rather than above it. Memory bandwidth is not the reason at 16 KiB. The 16 KiB input and 32 KiB of output fit in the 128 KB L1. The 64 MiB row is only a little slower, about 11×, so leaving L1 costs something, but it does not account for missing 18×.


## 3f — AI use disclosure

I used Cursor’s coding agent as a collaborator on this assignment. Together we organized the written responses, checked the machine and build details, and worked through analyzing the compiler report and the `to_hex` assembly for (a)–(c). We developed the Part 2 prediction and the answers for (d)–(h), which I then edited; the timings are from my own runs. We also debugged `to_hex_simd` after my first version failed to compile and only encoded one nibble, and implemented `to_hex_highway` the same way. I compiled and ran the program on my machine.
