# Activity 2 — Cache Set Conflicts Written Responses

## Part 1

### Machine geometry data: 

- first attempt (did not seem to work out cache geometry of my Apple M4 computer correctly)\
associativity not reported by the OS; measuring it...\
cpu0  4.42 GHz (measured, after warm-up)\
L1d: 128 KB, 1-way, 128-byte lines  ->  1024 sets

- second attempt (supplying known cache geometry)\
cpu0  4.44 GHz (measured, after warm-up)\
L1d: 128 KB, 8-way, 128-byte lines  ->  128 sets

(a) sets = size / (ways * line)
sets = 128 KB / (8-way * 128-byte lines) = 131,072 bytes / (8-way * 128-byte lines) = **128 sets**

(b) doubles per cache line = line size in bytes / 8
= 128 bytes / 8 = **16 doubles per line**

## Part 2

### Prediction Table

| `lda` | row bytes | sets reached (predicted) | fast or slow? (predicted) |
| --- | --- | --- | --- |
| 2048 | 16384 | 1 | slow |
| 2049 | 16392 | 128 | fast |
| 2050 | 16400 | 128 | fast |
| 2052 | 16416 | 128 | fast |
| 2056 | 16448 | 128 | fast |
| 2064 | 16512 | 128 | fast |
| 2080 | 16640 | 64  | slow |
| 2112 | 16896 | 32 | slow |
| 2176 | 17408 | 16 | slow |
| 2304 | 18432 | 8 | slow |

(c) 
I am using a 128-set threshold for slow vs. fast, based on whether as much of the column’s cache lines can survive in the sets it reaches. 

A column that reaches k sets can keep at most 8k of its lines resident at once, since each set holds 8 ways.
Each column touches 2,048 distinct cache lines (one per row, since rows are at least a line apart), while the whole L1 cache holds only 1024 lines (8-ways * 128 sets). So even perfect spreading can’t hold a full column in L1, and my threshold is about how much of it can survive in sets reached, not whether all of it fits. So no choice of lda can keep a full column in L1. What lda controls is how much of L1's capacity the column can use: reaching k sets gives 8k lines, and only k = 128 gives the full 8 × 128 = 1,024 L1 cache lines. Below 128 sets, part of L1 is unusable by the column, so it evicts itself earlier. For example, 64 sets gives only 512 lines, and 1 set gives only 8.

Thus, my rule predicts fast at 128 sets and slow below 128 sets. This is a strict column-capacity rule; but input traffic and other cache levels can affect the measured times. Rows classified as slow may still improve on lda=2048.

## Part 3

### Measured table

| `lda` | sets reached | measured time | speedup vs `lda`=2048 | prediction correct? |
| --- | --- | --- | --- | --- |
| 2048  | 1 | 18.0 ms | 1.00x | Yes |
| 2304  | 8 | 8.8 ms | 2.05x | Yes |
| 2176  | 16 | 7.9 ms | 2.26x | Yes |
| 2112  | 32 | 7.7 ms | 2.32x | Yes |
| 2080  | 64 | 7.4 ms | 2.42x | Yes |
| 2049  | 128 | 11.4 ms | 1.56x | No: predicted fast |
| 2050  | 128 | 5.6 ms | 3.24x | Yes (though this is on the lower end still)|
| 2052  | 128 | 2.7 ms | 6.57x | Yes |
| 2056  | 128 | 2.5 ms | 7.28x | Yes |
| 2064  | 128 | 2.5 ms | 7.26x | Yes |

(d) Time is not monotonic in sets reached. Among the rows that reach fewer than 128 sets it falls steadily (1 set: 18.0 ms; 8: 8.8; 16: 7.9; 32: 7.7; 64: 7.4). The 128-set rows, however, range from 11.4 ms (lda=2049) down to 2.5 ms (lda=2056, 2064), and lda=2049 is slower than the 64-set row, so reaching more sets does not guarantee a faster run on my machine.

As a result, the curve flattens between about 8 and 64 sets: going from 8 to 64 sets (an 8x increase) only improves time from 8.8 to 7.4 ms, while the first jump from 1 to 8 sets cuts time by half. But, the true plateau is at the bottom 2-3 ms mark. Here, the next real gain comes only from the 128-set rows, and then only for lda ≥ 2052 (2.7 ms at 2052, 2.5 ms at 2056 and 2064; 2049 and 2050 are slower). So, ultimately, reaching all 128 sets is necessary for the fastest times but not sufficient alone: my formula counts sets reached but not how the visits may be ordered, and L2 or TLB effects may also contribute.

(Note: The Part 4 P-core run repeats this setup and gave 2049 at 8.7 ms and 2050 at 6.3 ms, so individual rows vary by tens of percent between runs; 2049 was slower than every padded row ≥ 2052 in both runs.)

(e)
The actual rule is to avoid row strides that concentrate column writes in too few cache sets, causing conflict misses when competing lines exceed each set's associativity. For lda=2304 the stride is 144 cache lines, so adv = 144 % 128 = 16 and the column reaches 128 / gcd(16, 128) = 8 sets; lda=2049 has a fractional cache-line stride, which spreads the column across all 128 sets.

(NOTE: On my machine 2049 reached all 128 sets but was not among the fastest rows (see (d)), so again, reaching many sets is necessary but not, alone, sufficient for the fastest times.)


(f) The threshold from Part 2 was 128 sets. Since 2048 reaches only one set and 2049 reaches all 128, the smallest qualifying leading dimension is lda = 2049.

That adds one double per row:
extra memory = 2048 * 8 (bytes per double) = 16,384 bytes = 16 KiB per matrix

The percentage increase is:
(2049 - 2048) / 2048 * 100 = 0.048828125% or approx. 0.049%.

Running it (it is already in ldas[]) confirms it reaches 128 sets but only gives a 1.56x speedup, so it meets the threshold without being "fast".

Testing the next few values (2050: 3.24x, 2051 [briefly added in ldas array to test]: 4.45x, 2052: 6.57x), the speedup keeps rising with padding. The smallest padding that was undoubtedly fully effective in my measurements is lda=2052, which gives 6.57x for 4 extra elements per row (4/2048 ≈ 0.2% extra memory). Larger padding lda=2056 and 2064 add about 7.3x but cost more memory (~0.4% and 0.8% respectively).

## Part 4

raw output: 
```bash
PIN_CPU=2 L1_SIZE=131072 L1_WAYS=8 L1_LINE=128 ./activity2
cpu2  4.39 GHz (measured, after warm-up)
L1d: 128 KB, 8-way, 128-byte lines  ->  128 sets

matrix 2048x2048 doubles, naive transpose

     lda   row bytes      sets       time    vs lda=N
-------- ----------- --------- ---------- -----------
    2048       16384         1     18.1 ms       1.00x
    2049       16392       128      8.7 ms       2.09x
    2050       16400       128      6.3 ms       2.89x
    2052       16416       128      2.9 ms       6.22x
    2056       16448       128      2.5 ms       7.28x
    2064       16512       128      2.5 ms       7.15x
    2080       16640        64      7.7 ms       2.36x
    2112       16896        32      7.3 ms       2.49x
    2176       17408        16      8.1 ms       2.23x
    2304       18432         8      8.9 ms       2.04x

(checksum 0)
core clock at end of run: 4.27 GHz

PIN_CPU=6 L1_SIZE=65536 L1_WAYS=8 L1_LINE=128 ./activity2
cpu6  2.28 GHz (measured, after warm-up)
L1d: 64 KB, 8-way, 128-byte lines  ->  64 sets

matrix 2048x2048 doubles, naive transpose

     lda   row bytes      sets       time    vs lda=N
-------- ----------- --------- ---------- -----------
    2048       16384         1     35.4 ms       1.00x
    2049       16392        64      7.2 ms       4.94x
    2050       16400        64      8.9 ms       3.99x
    2052       16416        64      9.1 ms       3.87x
    2056       16448        64      9.6 ms       3.69x
    2064       16512        64      9.6 ms       3.67x
    2080       16640        32      9.9 ms       3.57x
    2112       16896        16     12.2 ms       2.91x
    2176       17408         8     19.0 ms       1.86x
    2304       18432         4     26.0 ms       1.36x

(checksum 0)
core clock at end of run: 2.22 GHz
```
NOTE: The size override above is important for each run (performance vs efficiency): starter always reads hw.perflevel0.l1dcachesize, even when requesting the efficiency cluster. On macOS, PIN_CPU requests a cluster through its QoS setting rather than pinning to a numbered core.

|  | P-core (PIN_CPU=2) | E-core (PIN_CPU=6) |
|---|---|---|
| Clock after warm-up | 4.39 GHz | 2.28 GHz |
| L1d size / ways / sets | 128 KB / 8 / 128 | 64 KB / 8 / 64 (ways set by hand; harness measured 6) |
| Time at lda = 2048 | 18.1 ms | 35.4 ms |
| Best time in the table | 2.5 ms (lda 2056, 2064) | 7.2 ms (lda 2049) |
| Speedup, worst to best | 7.28x | 4.94x |


(g) Yes, the set counts change, though the fast/slow classification doesn't. The formula depends on the line size, the set count and the element size. The E-core has the same 128-byte lines and 8 ways but half the size, so it has 64 sets instead of 128 and one full lap of the sets is 8192 bytes instead of 16384. Sets reached become: 2048 → 1, 2049/2050/2052/2056/2064 → 64 (all sets), 2080 → 32, 2112 → 16, 2176 → 8, 2304 → 4. Under my "fast if all sets are reached" rule the same rows are predicted fast and the same rows slow, but the numbers in the table are halved from 2080 up. (The E-core ways were set to 8 by hand: the harness measured 6, which can't be a real value for a 64 KB cache with 128-byte lines.)

(h) Slow/fast core time ratio at lda = 2048: 35.4 / 18.1 = 1.96. At the best lda: 7.2 / 2.5 = 2.88. Clock ratio: 4.39 / 2.28 = 1.93.

The lda = 2048 ratio is close to the clock ratio, so that case scales with the core clock: it is limited by the core and the caches next to it, where the repeated conflict misses are served in core cycles. The handout expects the best-lda ratio to be much smaller, but mine is larger than the clock ratio (2.88 vs 1.93). So the conflict-free E-core run is slower than clock alone explains, which suggests something other than the clock gets worse on the E-core. A candidate is its smaller L1 (64 KB), which holds only 512 lines of a 2048-line column per set-full of 64 sets, so more traffic goes to the L2. I have not tested that.

(i) No—the premise is shown to be the reverse:
In my data the padding speedup is larger on the faster core (7.28x vs 4.94x), so the statement does not hold on this machine, and this is consistent with (h). The speedup ratio between the cores is (35.4 / 7.2) / (18.1 / 2.5) = 0.68, which equals the 2048 time ratio divided by the best-lda time ratio (1.96 / 2.88). The slow core gets a larger speedup only if the best-lda ratio is smaller than the lda = 2048 ratio. On the M4 it is larger, so the E-core speedup is smaller.

## AI Disclosure

I used ChatGPT (Codex) and Claude to help understand the cache conflict model and my system's cache geometry in relation to the experiments being done, implement and troubleshoot my sets_reachable code, and interpret my benchmark results. It also helped organize my written answers/conclusions and explain where and possibly why my results differed from the handout’s expectations. I ran the experiments on my own machine and supplied the measured output.