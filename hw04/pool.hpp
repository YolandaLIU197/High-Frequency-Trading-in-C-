#pragma once
#include <cstddef>
#include <cstdint>
#include <new>

struct Pool {
    Pool(std::size_t obj_size, std::size_t capacity)
        : slot_(rounded_slot(obj_size)),
          capacity_(capacity),
          buffer_(static_cast<std::uint8_t*>(::operator new(slot_ * capacity_))),
          head_(nullptr) {
        for (std::size_t i = capacity_; i-- > 0; )
            free(buffer_ + i * slot_);
    }

    ~Pool() { ::operator delete(buffer_); }

    Pool(const Pool&) = delete;
    Pool& operator=(const Pool&) = delete;

    void* alloc() {
        if (!head_) return nullptr;
        void* p = head_;
        head_ = *static_cast<void**>(head_);
        return p;
    }

    void free(void* p) {
        if (!p) return;
        *static_cast<void**>(p) = head_;
        head_ = p;
    }

private:
    static std::size_t rounded_slot(std::size_t n) {
        const std::size_t a = alignof(void*);
        if (n < sizeof(void*)) n = sizeof(void*);
        return ((n + a - 1) / a) * a;
    }

    std::size_t slot_;
    std::size_t capacity_;
    std::uint8_t* buffer_;
    void* head_;
};
