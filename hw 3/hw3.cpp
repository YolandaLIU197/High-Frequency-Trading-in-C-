#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <iostream>
#include <memory>
#include <new>
#include <string>
#include <utility>
#include <vector>

#include "tests/bench.hpp"

struct Order {
    int id;
    double px;

    Order(int i, double p) : id(i), px(p) {}

    ~Order() {
        std::printf("~Order %d\n", id);
    }
};

bool risk_ok(double px) {
    return px < 1000.0;
}

void handle_raw(double px) {
    Order* o = new Order(1, px);

    if (!risk_ok(o->px)) {
        return;
    }

    std::printf("sent order %d @ %.2f\n", o->id, o->px);
    delete o;
}

void handle_unique(double px) {
    auto o = std::make_unique<Order>(2, px);

    if (!risk_ok(o->px)) {
        return;
    }

    std::printf("sent order %d @ %.2f\n", o->id, o->px);
}

class OrderGuard {
public:
    explicit OrderGuard(Order* p) : p_(p) {}

    ~OrderGuard() {
        delete p_;
    }

    OrderGuard(const OrderGuard&) = delete;
    OrderGuard& operator=(const OrderGuard&) = delete;

    Order* operator->() {
        return p_;
    }

private:
    Order* p_;
};

void handle_guard(double px) {
    OrderGuard o(new Order(3, px));

    if (!risk_ok(o->px)) {
        return;
    }

    std::printf("sent order %d @ %.2f\n", o->id, o->px);
}

class DynamicBuffer {
public:
    explicit DynamicBuffer(std::size_t n)
        : size_(n), data_(new double[n]) {}

    ~DynamicBuffer() {
        delete[] data_;
    }

    DynamicBuffer(const DynamicBuffer& other)
        : size_(other.size_),
          data_(new double[other.size_]) {
        std::copy(
            other.data_,
            other.data_ + size_,
            data_
        );
    }

    DynamicBuffer& operator=(const DynamicBuffer& other) {
        if (this == &other) {
            return *this;
        }

        double* new_data = new double[other.size_];

        std::copy(
            other.data_,
            other.data_ + other.size_,
            new_data
        );

        delete[] data_;

        data_ = new_data;
        size_ = other.size_;

        return *this;
    }

    double& operator[](std::size_t i) {
        return data_[i];
    }

    const double& operator[](std::size_t i) const {
        return data_[i];
    }

private:
    std::size_t size_;
    double* data_;
};

class SmartBuffer {
public:
    explicit SmartBuffer(std::size_t n)
        : size_(n),
          data_(std::make_unique<double[]>(n)) {}

    SmartBuffer(const SmartBuffer& other)
        : size_(other.size_),
          data_(std::make_unique<double[]>(other.size_)) {
        std::copy(
            other.data_.get(),
            other.data_.get() + size_,
            data_.get()
        );
    }

    SmartBuffer& operator=(const SmartBuffer& other) {
        if (this == &other) {
            return *this;
        }

        auto new_data =
            std::make_unique<double[]>(other.size_);

        std::copy(
            other.data_.get(),
            other.data_.get() + other.size_,
            new_data.get()
        );

        data_ = std::move(new_data);
        size_ = other.size_;

        return *this;
    }

    double& operator[](std::size_t i) {
        return data_[i];
    }

    const double& operator[](std::size_t i) const {
        return data_[i];
    }

private:
    std::size_t size_;
    std::unique_ptr<double[]> data_;
};

class ScopedTimer {
public:
    explicit ScopedTimer(const std::string& name)
        : name_(name),
          start_(std::chrono::steady_clock::now()) {}

    ~ScopedTimer() {
        auto end = std::chrono::steady_clock::now();

        double ms =
            std::chrono::duration<double, std::milli>(
                end - start_
            ).count();

        std::cout << name_
                  << ": "
                  << ms
                  << " ms\n";
    }

private:
    std::string name_;
    std::chrono::steady_clock::time_point start_;
};

class OrderPool {
public:
    void* alloc() {
        if (used_) {
            return nullptr;
        }

        used_ = true;
        return storage_;
    }

    void free(void* p) {
        if (p == storage_) {
            used_ = false;
        }
    }

private:
    alignas(Order) std::byte storage_[sizeof(Order)];
    bool used_ = false;
};

void pool_example(OrderPool& pool) {
    void* slot = pool.alloc();

    if (!slot) {
        return;
    }

    Order* o = new (slot) Order(4, 100.0);

    std::cout << "Pool order: "
              << o->px
              << "\n";

    o->~Order();
    pool.free(slot);
}

int main(int argc, char* argv[]) {
    if (argc > 1 && std::string(argv[1]) == "raw") {
        handle_raw(2000.0);
        return 0;
    }

    if (argc > 1 && std::string(argv[1]) == "fixed") {
        handle_unique(2000.0);
        handle_guard(2000.0);
        return 0;
    }

    handle_unique(2000.0);
    handle_unique(100.0);

    handle_guard(2000.0);
    handle_guard(100.0);

    DynamicBuffer a(3);
    a[0] = 101.5;
    a[1] = 102.5;
    a[2] = 103.5;

    DynamicBuffer b = a;
    b[0] = 999.0;

    DynamicBuffer& a_ref = a;
    a = a_ref;

    std::vector<DynamicBuffer> buffers;
    buffers.reserve(1);
    buffers.push_back(a);
    buffers.push_back(b);

    std::cout << "DynamicBuffer deep copy: "
              << a[0] << ", "
              << b[0] << "\n";

    std::cout << "DynamicBuffer vector size: "
              << buffers.size() << "\n";

    SmartBuffer s1(3);
    s1[0] = 10.5;
    s1[1] = 20.5;
    s1[2] = 30.5;

    SmartBuffer s2 = s1;
    s2[0] = 999.0;

    SmartBuffer s3(1);
    s3 = s1;

    SmartBuffer& s1_ref = s1;
    s1 = s1_ref;

    std::cout << "SmartBuffer deep copy: "
              << s1[0] << ", "
              << s2[0] << "\n";

    {
        ScopedTimer timer("RAII timer");

        volatile double sum = 0.0;

        for (int i = 0; i < 1'000'000; ++i) {
            sum += i * 0.001;
        }
    }

    constexpr std::size_t creation_iterations = 200'000;

    std::vector<std::unique_ptr<double>> unique_vec;
    unique_vec.reserve(creation_iterations);

    auto unique_start =
        std::chrono::steady_clock::now();

    for (std::size_t i = 0;
         i < creation_iterations;
         ++i) {
        unique_vec.push_back(
            std::make_unique<double>(123.45)
        );
    }

    auto unique_end =
        std::chrono::steady_clock::now();

    double unique_create =
        std::chrono::duration<double, std::nano>(
            unique_end - unique_start
        ).count() / creation_iterations;

    std::vector<std::shared_ptr<double>> shared_vec;
    shared_vec.reserve(creation_iterations);

    auto shared_start =
        std::chrono::steady_clock::now();

    for (std::size_t i = 0;
         i < creation_iterations;
         ++i) {
        shared_vec.push_back(
            std::make_shared<double>(123.45)
        );
    }

    auto shared_end =
        std::chrono::steady_clock::now();

    double shared_create =
        std::chrono::duration<double, std::nano>(
            shared_end - shared_start
        ).count() / creation_iterations;

    double raw_value = 123.45;
    double* raw = &raw_value;

    auto up = std::make_unique<double>(123.45);
    auto sp = std::make_shared<double>(123.45);

    constexpr std::size_t deref_iterations =
        50'000'000;

    double raw_deref = ns_per_op([&] {
        double v = *raw;
        doNotOptimize(v);
    }, deref_iterations);

    double unique_deref = ns_per_op([&] {
        double v = *up;
        doNotOptimize(v);
    }, deref_iterations);

    double shared_deref = ns_per_op([&] {
        double v = *sp;
        doNotOptimize(v);
    }, deref_iterations);

    double shared_copy = ns_per_op([&] {
        auto copy = sp;
        doNotOptimize(copy.get());
    }, deref_iterations);

    std::cout << "unique_ptr creation: "
              << unique_create
              << " ns/op\n";

    std::cout << "shared_ptr creation: "
              << shared_create
              << " ns/op\n";

    std::cout << "raw pointer dereference: "
              << raw_deref
              << " ns/op\n";

    std::cout << "unique_ptr dereference: "
              << unique_deref
              << " ns/op\n";

    std::cout << "shared_ptr dereference: "
              << shared_deref
              << " ns/op\n";

    std::cout << "shared_ptr copy: "
              << shared_copy
              << " ns/op\n";

    OrderPool pool;
    pool_example(pool);

    return 0;
}