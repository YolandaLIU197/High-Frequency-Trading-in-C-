# HW 3 — Dynamic Allocation, RAII & Smart Pointers

## Environment

- Machine: MacBook Pro with Apple M1
- Memory: 8 GB RAM
- OS: macOS 26.5.1
- Language: C++17
- Compiler: clang++
- Correctness / leak build: `-O0 -g`
- Benchmark build: `-O2`

## Build

For correctness and leak testing:

```bash
clang++ -std=c++17 -O0 -g hw3.cpp -o hw3_check
```

For benchmark measurements:

```bash
clang++ -std=c++17 -O2 -Wall -Wextra hw3.cpp -o hw3
./hw3
```

## Raw Allocation and RAII

`handle_raw` allocates an `Order` with `new`. If the risk check fails, the function returns before reaching `delete`, so the allocated object is leaked.

On macOS, I tested the raw version with:

```bash
leaks --atExit -- ./hw3_check raw
```

The result was:

```text
1 leak for 32 total leaked bytes
ROOT LEAK: <malloc in handle_raw(double)>
```

I then rewrote the same logic in two ways:

1. Using `std::unique_ptr`
2. Using a custom RAII owner whose destructor deletes the owned `Order`

I tested the fixed versions with:

```bash
leaks --atExit -- ./hw3_check fixed
```

The result was:

```text
0 leaks for 0 total leaked bytes
```

Both RAII versions release the owned object automatically when the scope exits, including the early-return path.

## Rule of Three

`DynamicBuffer` owns a dynamically allocated `double[]` using raw `new[]` and `delete[]`.

The class implements:

- a destructor
- a copy constructor
- a copy-assignment operator

Both copy operations perform deep copies, so two `DynamicBuffer` objects do not share the same underlying array.

The test produced:

```text
DynamicBuffer deep copy: 101.5, 999
DynamicBuffer vector size: 2
```

Changing the copied buffer did not modify the original buffer. The implementation also handled self-assignment and `std::vector` reallocation without data corruption or double-free behavior.

## `unique_ptr` Rewrite

I rewrote the buffer as `SmartBuffer` using:

```cpp
std::unique_ptr<double[]>
```

This removes the need to manually call `delete[]` and removes the need for a custom destructor whose only purpose is releasing the buffer.

The copy constructor and copy-assignment operator are still implemented because `std::unique_ptr` itself is non-copyable, while `SmartBuffer` is intended to preserve deep-copy behavior.

The test produced:

```text
SmartBuffer deep copy: 10.5, 999
```

This confirms that the copied `SmartBuffer` owns a separate array.

## RAII Timer

`ScopedTimer` records the starting time when it is constructed and reports elapsed time from its destructor.

One run produced:

```text
RAII timer: 4.08737 ms
```

Because the timing action is tied to the object's lifetime, the destructor is automatically called when the scope exits.

## Smart Pointer Benchmarks

The benchmark was compiled with `-O2`.

Results from one run:

| Operation | Time |
|---|---:|
| `unique_ptr` creation | 17.0431 ns/op |
| `shared_ptr` creation | 21.2898 ns/op |
| raw pointer dereference | 0.406339 ns/op |
| `unique_ptr` dereference | 0.458483 ns/op |
| `shared_ptr` dereference | 0.397334 ns/op |
| `shared_ptr` copy | 13.4183 ns/op |

The raw pointer, `unique_ptr`, and `shared_ptr` dereference measurements are all approximately the same. The small differences between them are measurement noise rather than meaningful ownership overhead.

The main additional cost of `shared_ptr` appears when it is copied. Copying a `shared_ptr` does not copy the underlying object, but it updates the reference count in the shared control block. The increment and later decrement of this reference count are atomic operations, which adds overhead.

`shared_ptr` creation was also more expensive than `unique_ptr` creation because shared ownership requires additional control-block state.

`unique_ptr` is move-only and cannot be copied because it represents exclusive ownership.

## Benchmark Method

My first `unique_ptr` creation benchmark produced an unrealistically low value of about `0.3 ns/op`. Under `-O2`, the compiler was able to optimize away the short-lived allocation.

I changed the creation benchmark so that the created smart pointers remain alive in pre-reserved vectors during the timed section. This prevented the allocations from being removed by the optimizer and produced the measurements reported above.

## Pool Allocation and the Hot Path

The hot-path example replaces repeated heap allocation with a pre-allocated pool slot.

The basic pattern is:

```cpp
void* slot = pool.alloc();
if (!slot) return;

Order* o = new (slot) Order(4, 100.0);

o->~Order();
pool.free(slot);
```

The test produced:

```text
Pool order: 100
```

This pattern avoids calling `new` or `make_shared` for every event on the hot path. Heap allocation can involve allocator synchronization or page faults, which may happen only occasionally. Because of this, allocation can have a much larger effect on p99.9 tail latency than on median p50 latency.

## Summary

The raw-pointer example shows how manual ownership can leak when control flow exits early. RAII ties resource lifetime to object lifetime and makes cleanup automatic.

For exclusive ownership, `std::unique_ptr` removes manual memory cleanup with essentially no dereference overhead. `std::shared_ptr` is useful when ownership is genuinely shared, but copying it has additional atomic reference-count cost.

In latency-sensitive code, unnecessary shared ownership and heap allocation should therefore be avoided on the hot path.