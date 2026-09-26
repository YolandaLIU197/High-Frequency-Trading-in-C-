# HW 2 — Pointers, References & the Cost of a Copy

## Machine and build

| Item | Configuration |
| --- | --- |
| Machine | MacBook Pro (MacBookPro17,1) |
| CPU | Apple M1, 8 cores (4 performance, 4 efficiency), arm64 |
| Memory | 8 GB |
| OS | macOS 26.5.1 (build 25F80) |
| Compiler | Apple clang 15.0.0 (clang-1500.3.9.4), via `c++` |
| Language and flags | `-std=c++17 -O2 -Wall -Wextra` (release build) |

I ran `make` in `starters/hw02` and report the release-build output below. The provided harness uses `std::chrono::steady_clock`, warms up with whole batches before sampling, and applies `doNotOptimize()` to the results. Table 2 uses 1,000 samples of 2,000 calls per sample; Table 3 uses 200 samples of one full traversal each. The tables report p50, p99, p99.9, and mean. These are results from one run and may vary with background activity and system conditions. I did not otherwise control background activity during the run.


## Release run output

The following is the program output from the release run, including the correctness checks, ratios, sample counts, and method details:

```text
HW 2 — pointers, references & the cost of a copy
elements N = 1048576   sizeof(Big) = 648 B   sizeof(Node) = 16 B

TABLE 1 — swap correctness
  function           a before   b before    a after    b after   result
  ---------------------------------------------------------------------
  swap_ref                  3          9          9          3   OK
  swap_ptr                  3          9          9          3   OK
  swap_ptr(&a,&a)           5          5          5          5   OK

TABLE 2 — pass a 648-byte struct: by value vs by const reference
  variant                             p50          p99        p99.9         mean
                                  ns/call      ns/call      ns/call      ns/call
  ---------------------------------------------------------------------------------
  sum_by_value(Big)                47.562       66.688      114.229       49.798
  sum_by_cref(const Big&)          33.208       34.209       39.667       33.260
  p50 ratio value/cref = 1.43x     (checksum 30202200000.0, 1000 samples x 2000 calls)
  correctness: sum_by_value=7191.0  sum_by_cref=7191.0  OK (equal, non-zero)

TABLE 3 — traverse 1,048,576 ints: contiguous vector vs linked list
  variant                             p50          p99        p99.9         mean
                                  ns/elem      ns/elem      ns/elem      ns/elem
  ---------------------------------------------------------------------------------
  sum_vector (contiguous)           0.047        0.064        0.084        0.049
  sum_list   (pointer chase)       22.482       56.322       59.715       25.994
  p50 ratio list/vector = 478.24x    (200 samples x 1 full traversal)
  correctness: sum_vector=549755289600  sum_list=549755289600  expected=549755289600  OK
  bytes touched: vector 4.0 MB, list 16.0 MB

TABLE 4 — build & method (state your machine in the README)
  compiler               clang++ 15.0.0 (clang-1500.3.9.4)
  optimisation           -O2/-O3 (release)  OK
  language               __cplusplus = 201703L
  arch                   arm64
  clock                  std::chrono::steady_clock (monotonic)
  warm-up                yes — whole batches before the first sample
  reported               p50 / p99 / p99.9 over samples, plus mean
  dead-code guard        doNotOptimize() on every result
  machine                TODO(4): put your CPU / RAM / OS in the README
```

The `machine` TODO in Table 4 is a reminder printed by the provided program; my CPU, RAM, and OS are listed in the Machine and build section above.

## Explanation A — References and pointers

`swap_ref(int& a, int& b)` receives references to the caller's integers, so assigning to `a` and `b` changes the original values. `swap_ptr(int* a, int* b)` receives copies of their addresses. It must use `*a` and `*b` to change the integers at those addresses; swapping the local pointer variables would not change the caller's integers. Both approaches access the existing integers rather than copying them into separate objects for the swap. A pointer may be `nullptr`, so I check both pointers and return early before dereferencing if either is null. A valid C++ reference must refer to an object, so a caller should not use a reference to represent an absent value. The temporary-value swap also leaves the integer unchanged when both pointer arguments refer to the same integer.

In an HFT hot path, I would use a reference when the function requires an existing object, because the interface expresses that requirement without a nullable argument. I would use a pointer if `nullptr` has a useful meaning. Table 1 tests whether the swaps work; it does not measure whether the reference or pointer version is faster.


## Explanation B — Copies and memory layout

The two `Big` functions perform the same arithmetic, but the by-value call passes a 648-byte object while the `const`-reference call accesses the existing object without making that argument copy. The by-value p50 was 47.562 ns/call, versus 33.208 ns/call by reference, a difference of 14.354 ns/call and a ratio of 1.43×. Dividing 648 bytes by that difference gives roughly 45 GB/s if I count the object once, which is a plausible order of magnitude for a small copy served from cache. This is not a direct bandwidth measurement, since the timing also includes compiler and function-call effects. 

The vector stores adjacent four-byte integers. A nominal 64-byte cache line can therefore bring in 16 values that the traversal will use. Its sequential access pattern also helps the hardware prefetch future lines. Each linked-list `Node` is 16 bytes, but only its four-byte `v` field contributes to the sum; the `next` pointer is needed to continue the traversal. Because the nodes are followed in shuffled order, fetching a line containing the current node does not usually provide the next node. The processor must load `p->next` before it knows the next address, creating a dependent-load chain that limits prefetching and the overlap of cache misses. This helps explain the measured p50 of 22.482 ns/element for the list versus 0.047 ns/element for the vector. The 478.24× ratio belongs to this particular benchmark and machine. This is also why a flat, price-indexed order book can be preferable to a node-based map in a latency-sensitive hot path: it keeps accesses more predictable and avoids pointer chasing.

I also ran `make debug`. Its `-O0` build produced correct answers, but I excluded those timings from the reported release benchmark because unoptimized loop and call overhead change what the comparison measures.