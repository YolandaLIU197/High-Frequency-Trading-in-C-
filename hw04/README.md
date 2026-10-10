# HW4 — High-Performance Allocators and Runtime Polymorphism

## Environment

- MacBook Pro, Apple M1, 8 GB RAM, macOS 26.5.1
- Apple Clang, C++17, `-O2`

All benchmark results below were measured on my Mac using the combined `main.cpp`.

## Files and Running

- `pool.hpp` — fixed-size memory pool using an intrusive free-list
- `arena.hpp` — bump allocator that releases memory with `reset()`
- `bench.hpp` — provided benchmarking helper
- `main.cpp` — allocator comparison, polymorphism example, and virtual-call benchmark

Compile and run from the HW4 folder:

```bash
clang++ -std=c++17 -O2 -Wall -Wextra -I. main.cpp -o hw4_run
./hw4_run
```

## 1. Free-List Pool

I used one preallocated buffer and linked the available slots together using an intrusive free-list. `alloc()` takes a slot from the head, and `free()` puts it back. Both operations are O(1), and `alloc()` returns `nullptr` when the pool is full.

All four provided Pool tests passed: `pool_basic`, `pool_full`, `pool_reuse`, and `pool_pattern` (100k fill/drain rounds). In the starter repository, `make pool` measured **0.432291 ns/op**. The full `make test` run also passed all Pool checks (**25/25**) and measured **0.320125 ns/op** for its Pool metric. These are separate measurements from the comparison below.

## 2. Arena Allocator and Allocation Benchmark

For the Arena, I used an offset that moves forward on each allocation, with alignment handled before returning the address. Instead of freeing each object, `reset()` sets the offset back to zero after a batch.

I compared `malloc/free`, Pool, and Arena with 1,024 allocations of 64 bytes per batch, 3,000 batches per run, 100 warm-up batches, and the median of 7 runs. Each result includes allocation and reclamation cost, averaged per allocation.

| Allocator | ns/op | Speedup vs. malloc/free |
| --- | ---: | ---: |
| malloc/free | 21.486 | 1.00× |
| Pool | 1.721 | 12.49× |
| Arena | 1.318 | 16.30× |

Both custom allocators were much faster for this workload. The Pool only needs to update pointers in the free-list, so it avoids general-purpose heap allocation and has fairly predictable work per operation. The Arena was fastest here because allocation mainly advances an offset and the entire batch is reclaimed by one reset. This is useful for short-lived temporary data, although Arena cannot free individual allocations.

## 3. Runtime Polymorphism

I created an abstract `RiskCheck` interface with two implementations: `QuantityLimit` and `NotionalLimit`. Both are constructed inside Pool slots using placement `new` and called through `RiskCheck*` pointers. Afterward, I explicitly call the virtual destructor and return each slot to the Pool.

`NotionalLimit` also owns a tracked buffer, so I checked that the derived destructor releases its resource correctly.

```text
Approved: true
Approved: false
Live objects after teardown: 0
Live buffers after teardown: 0
POLYMORPHISM CHECK: PASS
```

Both counters returned to zero. This checks the object and tracked-buffer lifetimes, though it is not a full sanitizer check.

## 4. Virtual-Call Benchmark

I measured direct calls, one-target virtual calls, four-target virtual calls (predictable and pseudorandom order), and calls through a `final` derived class. Each case used 5 million calls per run, 2 warm-ups, and the median of 7 runs. The benchmark keeps the computed results observable so the compiler cannot simply remove the work.

| Call type | ns/call | Relative to direct |
| --- | ---: | ---: |
| Direct | 0.320 | 1.00× |
| Virtual, 1 target | 0.961 | 3.00× |
| Virtual, 4 predictable | 2.533 | 7.92× |
| Virtual, 4 pseudorandom | 6.503 | 20.32× |
| `final` (derived pointer) | 0.526 | 1.64× |

The pseudorandom virtual calls were the slowest. With virtual dispatch, the function target may need to be loaded through a vtable, and unpredictable targets are harder for the CPU's branch predictor/BTB to predict. A wrong prediction adds extra work. Virtual dispatch can also limit inlining. The `final` case was much closer to the direct call because the compiler knew the derived type and could optimize the call. The benchmark includes loop and function-body costs as well, so the full gap is not just the cost of one virtual instruction.

### Assembly Check

I generated ARM64 assembly on my M1 with:

```bash
clang++ -std=c++17 -O2 -S main.cpp -o hw4.s
```

I checked the optimized benchmark loops, rather than counting calls in the entire assembly file:

| Case | Loop label | `bl` | `blr` |
| --- | --- | ---: | ---: |
| Direct | `LBB6_11` | 0 | 0 |
| Virtual, 1 target | `LBB6_44` | 0 | 1 |
| Virtual, 4 predictable | `LBB6_78` | 0 | 1 |
| Virtual, 4 pseudorandom | `LBB6_113` | 0 | 1 |
| `final` | `LBB6_148` | 0 | 0 |

The three virtual loops each use `blr x8` for an indirect call after loading the function pointer. The direct and `final` loops have no call instruction inside the hot loop, which is consistent with inlining/devirtualization in this build.
