# HW 3 — Dynamic Allocation, RAII & Smart Pointers

## Environment

- MacBook Pro, Apple M1
- 8 GB RAM
- macOS 26.5.1
- C++17
- clang++
- Correctness / leak build: `-O0 -g`
- Benchmark build: `-O2`

## Build

For leak testing:

```bash
clang++ -std=c++17 -O0 -g hw3.cpp -o hw3_check
```

For benchmark testing:

```bash
clang++ -std=c++17 -O2 -Wall -Wextra hw3.cpp -o hw3
./hw3
```

## Raw Allocation and RAII

I first used a raw pointer in `handle_raw`. If the function returns early, the allocated `Order` is not deleted.

I checked it on macOS with:

```bash
leaks --atExit -- ./hw3_check raw
```

Result:

```text
1 leak for 32 total leaked bytes
ROOT LEAK: <malloc in handle_raw(double)>
```

I then rewrote it with `std::unique_ptr` and also with a custom RAII guard.

```bash
leaks --atExit -- ./hw3_check fixed
```

Result:

```text
0 leaks for 0 total leaked bytes
```

Both versions cleaned up the object automatically even when the function returned early.

## Rule of Three

I created `DynamicBuffer` using raw `new[]` and `delete[]`.

I implemented:

- destructor
- copy constructor
- copy assignment

The copy constructor and assignment both make deep copies.

Result:

```text
DynamicBuffer deep copy: 101.5, 999
DynamicBuffer vector size: 2
```

Changing the copied buffer did not change the original. I also tested self-assignment and `std::vector` reallocation.

## `unique_ptr` Version

I rewrote the buffer as `SmartBuffer` using:

```cpp
std::unique_ptr<double[]>
```

With `unique_ptr`, I no longer need to manually call `delete[]` or write a destructor just for memory cleanup.

I still kept the copy constructor and copy assignment because `unique_ptr` itself cannot be copied, but I still wanted `SmartBuffer` to support deep copy.

Result:

```text
SmartBuffer deep copy: 10.5, 999
```

## RAII Timer

I created a `ScopedTimer` that records the start time when it is created and prints the elapsed time in its destructor.

Result from one run:

```text
RAII timer: 4.08737 ms
```

## Smart Pointer Benchmarks

Results from one `-O2` run:

| Operation | Time |
|---|---:|
| `unique_ptr` creation | 17.0431 ns/op |
| `shared_ptr` creation | 21.2898 ns/op |
| raw pointer dereference | 0.406339 ns/op |
| `unique_ptr` dereference | 0.458483 ns/op |
| `shared_ptr` dereference | 0.397334 ns/op |
| `shared_ptr` copy | 13.4183 ns/op |

The three dereference results were very close, so there was not much difference between raw pointer, `unique_ptr`, and `shared_ptr` dereference.

The main extra cost came from copying a `shared_ptr`, since the reference count has to be updated atomically.

`shared_ptr` creation was also a little slower than `unique_ptr` creation because it needs extra shared-ownership information.

## Benchmark Note

My first `unique_ptr` creation result was around `0.3 ns/op`, which was clearly too low.

The compiler was optimizing away the short-lived allocation under `-O2`, so I changed the benchmark to keep the created smart pointers alive in pre-reserved vectors.

After that, the results became more reasonable.

## Pool Allocation

I also tested the idea of replacing heap allocation in a hot path with a pre-allocated pool slot.

```cpp
void* slot = pool.alloc();
if (!slot) return;

Order* o = new (slot) Order(4, 100.0);

o->~Order();
pool.free(slot);
```

Result:

```text
Pool order: 100
```

This avoids calling `new` or `make_shared` every time the hot path runs. This matters more for tail latency such as p99.9 because heap allocation can occasionally be much slower than usual.

## Summary

This homework showed the difference between manual memory management and RAII.

The raw-pointer version could leak when the function returned early, while `unique_ptr` and the custom RAII guard cleaned up automatically.

For the buffer class, the Rule of Three was needed when using raw memory. After switching to `unique_ptr`, manual cleanup was no longer needed.

The benchmark also showed that pointer dereference itself is cheap, while `shared_ptr` copying has extra cost because of reference counting.

AddressSanitizer was also attempted on macOS, but the sanitizer runtime failed during initialization on my system. I therefore used macOS `leaks` for leak checking and separately tested the Rule-of-Three implementation with self-assignment and `std::vector` reallocation.