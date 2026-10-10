#include "pool.hpp"
#include "arena.hpp"
#include "bench.hpp"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <new>
#include <stdexcept>
#include <vector>

namespace allocator {

constexpr std::size_t OBJECT_BYTES = 64;
constexpr std::size_t BATCH = 1024;
constexpr int BATCHES = 3000; 
constexpr int REPS = 7;


static void* slots[BATCH];

void malloc_batch() {
    for (std::size_t i = 0; i < BATCH; ++i) {
        slots[i] = std::malloc(OBJECT_BYTES);
        if (!slots[i]) throw std::bad_alloc();
        doNotOptimize(slots[i]);
    }
    for (std::size_t i = 0; i < BATCH; ++i) std::free(slots[i]);
    clobber();
}

void pool_batch(Pool& pool) {
    for (std::size_t i = 0; i < BATCH; ++i) {
        slots[i] = pool.alloc();
        if (!slots[i]) throw std::runtime_error("Pool unexpectedly exhausted");
        doNotOptimize(slots[i]);
    }
    for (std::size_t i = 0; i < BATCH; ++i) pool.free(slots[i]);
    clobber();
}

void arena_batch(Arena& arena) {
    for (std::size_t i = 0; i < BATCH; ++i) {
        slots[i] = arena.alloc(OBJECT_BYTES);
        if (!slots[i]) throw std::runtime_error("Arena unexpectedly exhausted");
        doNotOptimize(slots[i]);
    }
    arena.reset(); 
    clobber();
}

template <typename F>
double measure_median(F&& batch_fn) {
    for (int i = 0; i < 100; ++i) batch_fn(); 
    std::vector<double> measurements;
    measurements.reserve(REPS);
    for (int r = 0; r < REPS; ++r) {
        auto start = std::chrono::steady_clock::now();
        for (int b = 0; b < BATCHES; ++b) batch_fn();
        auto end = std::chrono::steady_clock::now();
        const double ns = std::chrono::duration<double, std::nano>(end - start).count();
        measurements.push_back(ns / (static_cast<double>(BATCHES) * BATCH));
    }
    std::sort(measurements.begin(), measurements.end());
    return measurements[REPS / 2];
}

void run_allocator_benchmark() {
    Pool pool(OBJECT_BYTES, BATCH);
    Arena arena(OBJECT_BYTES * BATCH);

    const double malloc_ns = measure_median(malloc_batch);
    const double pool_ns = measure_median([&] { pool_batch(pool); });
    const double arena_ns = measure_median([&] { arena_batch(arena); });

    std::cout << "Workload: " << BATCH << " x " << OBJECT_BYTES
              << " bytes per batch, " << BATCHES << " batches per run\n";
    std::cout << "Metric: ns per allocation+reclamation, median of " << REPS << " runs\n";
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "malloc/free: " << malloc_ns << " ns/op (baseline)\n";
    std::cout << "Pool:        " << pool_ns << " ns/op (" << malloc_ns / pool_ns << "x speedup)\n";
    std::cout << "Arena:       " << arena_ns << " ns/op (" << malloc_ns / arena_ns << "x speedup)\n";
}
} 

namespace poly {

struct RiskCheck {
    static int live_objects;
    RiskCheck() { ++live_objects; }
    virtual ~RiskCheck() { --live_objects; }
    virtual bool approve(double price, int quantity) const = 0;
};
int RiskCheck::live_objects = 0;

struct QuantityLimit final : RiskCheck {
    int max_quantity;
    explicit QuantityLimit(int limit) : max_quantity(limit) {}
    bool approve(double, int quantity) const override {
        return quantity > 0 && quantity <= max_quantity;
    }
};


struct TrackedBuffer {
    static int live_buffers;
    std::unique_ptr<char[]> bytes;
    TrackedBuffer() : bytes(new char[256]) { ++live_buffers; }
    ~TrackedBuffer() { --live_buffers; }
    TrackedBuffer(const TrackedBuffer&) = delete;
    TrackedBuffer& operator=(const TrackedBuffer&) = delete;
};
int TrackedBuffer::live_buffers = 0;

struct NotionalLimit final : RiskCheck {
    double max_notional;
    TrackedBuffer scratch;
    explicit NotionalLimit(double limit) : max_notional(limit) {}
    bool approve(double price, int quantity) const override {
        return price > 0 && quantity > 0 && price * quantity <= max_notional;
    }
};

void run_polymorphism() {
    constexpr std::size_t SLOT_SIZE =
        sizeof(QuantityLimit) > sizeof(NotionalLimit)
            ? sizeof(QuantityLimit) : sizeof(NotionalLimit);
    static_assert(alignof(QuantityLimit) <= alignof(std::max_align_t), "over-aligned type");
    static_assert(alignof(NotionalLimit) <= alignof(std::max_align_t), "over-aligned type");
    Pool pool(SLOT_SIZE, 2);

    void* first = pool.alloc();
    void* second = pool.alloc();
    if (!first || !second) throw std::runtime_error("Pool did not provide two slots");

    RiskCheck* checks[2] = {
        new (first) QuantityLimit(100),
        new (second) NotionalLimit(5000.0)
    };

    const double price = 60.0;
    const int quantity = 90;
    for (const RiskCheck* check : checks)
        std::cout << "Approved: " << std::boolalpha
                  << check->approve(price, quantity) << '\n';

    for (RiskCheck* check : checks) {
        check->~RiskCheck();  
        pool.free(check);     
    }

    std::cout << "Live objects after teardown: " << RiskCheck::live_objects << '\n';
    std::cout << "Live buffers after teardown: " << TrackedBuffer::live_buffers << '\n';
    if (RiskCheck::live_objects != 0 || TrackedBuffer::live_buffers != 0)
        throw std::runtime_error("Polymorphic lifetime check failed");
    std::cout << "POLYMORPHISM CHECK: PASS\n";
}
} 

namespace dispatch {


struct RiskCheck {
    virtual ~RiskCheck() = default;
    virtual bool approve(double price, int quantity) const = 0;
};

struct QuantityLimit final : RiskCheck {
    int limit = 100;
    bool approve(double, int q) const override { return q > 0 && q <= limit; }
};
struct NotionalLimit final : RiskCheck {
    double limit = 5000.0;
    bool approve(double p, int q) const override { return p * q <= limit && q > 0; }
};
struct PriceLimit final : RiskCheck {
    double limit = 70.0;
    bool approve(double p, int q) const override { return p <= limit && q > 0; }
};
struct MinQuantity final : RiskCheck {
    int minimum = 20;
    bool approve(double, int q) const override { return q >= minimum; }
};


struct DirectQuantity {
    int limit = 100;
    bool approve(double, int q) const { return q > 0 && q <= limit; }
};


static volatile std::uint64_t sink = 0;
constexpr int N = 5'000'000;
constexpr int REPS = 7;

template <class F>
double median_ns(F&& f) {
    for (int i = 0; i < 2; ++i) f();
    std::vector<double> times;
    times.reserve(REPS);
    for (int r = 0; r < REPS; ++r) {
        const auto begin = std::chrono::steady_clock::now();
        f();
        const auto end = std::chrono::steady_clock::now();
        times.push_back(std::chrono::duration<double, std::nano>(end - begin).count() / N);
    }
    std::sort(times.begin(), times.end());
    return times[REPS / 2];
}

void run_virtual_benchmark() {
    QuantityLimit a;
    NotionalLimit b;
    PriceLimit c;
    MinQuantity d;
    DirectQuantity direct;
    const RiskCheck* targets[4] = {&a, &b, &c, &d};

    unsigned char pattern[4096];
    std::uint32_t state = 12345;
    for (auto& id : pattern) {
        state = state * 1664525u + 1013904223u;
        id = static_cast<unsigned char>((state >> 24) & 3u);
    }


    const RiskCheck* volatile one = &a;
    const QuantityLimit* volatile known_final = &a;

    auto run = [&](auto&& invoke) {
        return median_ns([&] {
            std::uint64_t accepted = 0;
            for (int i = 0; i < N; ++i) {
                const int quantity = 10 + (i & 127);
                const double price = 59.0 + (i & 15) * 0.25;
                accepted += static_cast<std::uint64_t>(invoke(price, quantity, i));
                sink = accepted;  
            }
        });
    };

    const double t_direct = run([&](double p, int q, int) { return direct.approve(p, q); });
    const double t_one = run([&](double p, int q, int) { return one->approve(p, q); });
    const double t_predictable = run([&](double p, int q, int i) {
        return targets[i & 3]->approve(p, q);
    });
    const double t_random = run([&](double p, int q, int i) {
        return targets[pattern[i & 4095]]->approve(p, q);
    });
    const double t_final = run([&](double p, int q, int) {
        return known_final->approve(p, q);
    });

    std::cout << "Workload: " << N << " calls/run, " << REPS
              << " timed runs after 2 warmups; median ns/call\n";
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "Direct call:                  " << t_direct << " ns/call\n";
    std::cout << "Virtual, 1 target:            " << t_one << " ns/call\n";
    std::cout << "Virtual, 4 predictable:       " << t_predictable << " ns/call\n";
    std::cout << "Virtual, 4 pseudorandom:      " << t_random << " ns/call\n";
    std::cout << "Final class through derived*: " << t_final << " ns/call\n";
    std::cout << "sink=" << sink << "\n";
}
} 

int main() {
    std::cout << "Allocator comparison\n";
    allocator::run_allocator_benchmark();
    std::cout << "\nRuntime polymorphism\n";
    poly::run_polymorphism();
    std::cout << "\nVirtual-call comparison\n";
    dispatch::run_virtual_benchmark();
}
