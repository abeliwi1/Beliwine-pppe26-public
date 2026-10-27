# Sharing data between threads: Apple M5 results

The same measurements as [README.sharing.md](README.sharing.md), on an Apple
M5 instead of the Ryzen AI 9 HX 370. The methods are the same; what differs is
how the threads are placed, and the size of a cache line.

```
make the binary:  ~/opencilk/bin/clang++ -fopencilk -O3 -std=c++17 sharing.M5.cpp -o sharing.M5
one measurement:  CILK_NWORKERS=2 ./sharing.M5 P P false
everything:       ./run.M5.sh
```

## What's different on the M5

* **No pinning.** macOS can't put a thread on a chosen CPU, so each player
  asks for a *kind* of core with its QoS class: `USER_INTERACTIVE` for a
  performance core (P), `BACKGROUND` for an efficiency core (E). Background
  threads run only on the efficiency cores.
* **Three pairs, not four.** The M5's cores have no SMT, so there is no "same
  core" pair.

  | pair | arguments | what the two threads share |
  |---|---|---|
  | two performance cores | `P P` | the 16 MB L2 of the 4 performance cores |
  | two efficiency cores | `E E` | the 6 MB L2 of the 6 efficiency cores |
  | one of each | `P E` | nothing: the line crosses between the clusters |

* **128-byte cache lines.** The M5's line is 128 bytes (`sysctl
  hw.cachelinesize`), so `sharing.M5.cpp` aligns everything that must be alone
  on its line to 128. With the Ryzen's 64, the two "padded" counters would
  share a line on the M5.
* **Each player reports its clock.** macOS can't say which core a thread ran
  on, so each player times a chain of dependent adds, one per cycle, before
  it starts. In 65 of the 72 measurements below, a performance-core player
  measured 4.0 GHz or more, which confirms the placement; the other 7 measured
  2.2–3.9 GHz, mostly in the `P E` pair.

## Results

Measured 2026-10-23 on battery power (Low Power Mode off). Three runs of
`run.M5.sh`; each cell is the median of the three, with the range in
parentheses. As on the Ryzen, every run reports the fastest of 7 trials.

| mode | P + P | E + E | P + E |
|---|---|---|---|
| `pause` (ns per handoff) | 52.5 (51.8–52.6) | 76.8 (65.9–80.8) | 152.8 (140.8–156.9) |
| `spin` (ns per handoff) | 52.0 (51.5–52.2) | 64.7 (58.4–68.5) | 137.5 (134.6–138.4) |
| `false` (ns per increment) | 1.0 (1.0–1.0) | 3.7 (3.5–4.1) | 3.9 (3.8–4.1) |
| `padded` | 0.3 (0.3–0.3) | 3.4 (3.2–3.6) | 3.9 (3.6–4.7) |
| `false-fence` | 18.0 (17.3–19.1) | 13.4 (10.8–14.5) | 15.2 (14.7–15.4) |
| `padded-fence` | 1.4 (1.4–1.4) | 2.8 (2.5–3.6) | 3.3 (2.8–3.6) |
| `false-atomic` | 10.1 (9.4–10.5) | 11.3 (8.9–12.1) | 10.0 (9.9–10.4) |
| `padded-atomic` | 1.6 (1.6–1.6) | 2.7 (2.6–3.3) | 3.2 (3.0–3.2) |

**The efficiency cores ran slowly.** Background QoS also lowers the
efficiency cores' clock: their players measured 0.9–2.5 GHz, and varied from
run to run, so the `E + E` and `P + E` columns spread more than `P + P`. In
the increment modes, a `P + E` result is set by the slower player, the one on
the efficiency core.

### What the numbers say

**True sharing: about 52 ns within the performance cluster, about 150 ns
between clusters.** Passing a cache line between two performance cores costs
about 52 ns per handoff, roughly twice the Ryzen's 28 ns between two Zen 5
cores. Between a performance core and an efficiency core it costs about
150 ns, close to the Ryzen's 165 ns between its two clusters: on both
machines, crossing clusters costs about three to six times as much as staying
in one.

**Spinning hurts less here.** On the M5, `spin` is no slower than `pause`
(52.0 against 52.5 ns on two performance cores), where on the Ryzen it took up
to 2.7× as long. Not yet explained.

**Plain false sharing is cheap, as on the Ryzen.** On two performance cores, a
`false` increment costs 1.0 ns against 0.3 ns padded: the store buffer batches
the writes, so each transfer of the line is shared by many increments.

**False sharing hurts when each update must complete.** On two performance
cores, a fenced increment costs 18.0 ns when the counters share a line and
1.4 ns when they don't, about 13 times as much. An atomic `fetch_add` costs
10.1 against 1.6 ns, about 6 times. On the efficiency cores the gap is
smaller, because their slow clock makes even the unshared update expensive.

## Caveats

* QoS placement is a request, not a guarantee: a `USER_INTERACTIVE` thread can
  land on an efficiency core if the performance cores are busy. Each player's
  measured clock is the check; close other programs before measuring.
* `E` measures the efficiency cores at the clock macOS gives background work,
  not at their top speed.
* Measured on battery, like the Ryzen's provisional results. Re-measure on the
  mains adapter.
