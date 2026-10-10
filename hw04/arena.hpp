#pragma once
#include <cstddef>
#include <new>

struct Arena {
    explicit Arena(std::size_t capacity)
        : capacity_(capacity),
          buffer_(static_cast<std::byte*>(::operator new(capacity))),
          offset_(0) {}

    ~Arena() { ::operator delete(buffer_); }

    Arena(const Arena&) = delete;
    Arena& operator=(const Arena&) = delete;

    void* alloc(std::size_t bytes) {
        constexpr std::size_t align = alignof(std::max_align_t);
        const std::size_t padding = (align - (offset_ % align)) % align;
        if (padding > capacity_ - offset_ ||
            bytes > capacity_ - offset_ - padding) {
            return nullptr;
        }
        const std::size_t start = offset_ + padding;
        offset_ = start + bytes;
        return buffer_ + start;
    }

    void reset() { offset_ = 0; }

    std::size_t used() const { return offset_; }
    std::size_t capacity() const { return capacity_; }

private:
    std::size_t capacity_;
    std::byte* buffer_;
    std::size_t offset_;
};
