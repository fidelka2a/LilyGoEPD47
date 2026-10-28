#ifndef CORE_FRAMEBUFFER_H
#define CORE_FRAMEBUFFER_H

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

namespace core {

template <typename T>
class Span {
  public:
    Span(T* data, std::size_t size) : data_(data), size_(size) {}

    T* data() const { return data_; }
    std::size_t size() const { return size_; }
    T& operator[](std::size_t idx) const { return data_[idx]; }

  private:
    T* data_;
    std::size_t size_;
};

class Framebuffer {
  public:
    explicit Framebuffer(std::size_t sizeInBytes = 0) : buffer_(sizeInBytes, 0) {}

    void resize(std::size_t sizeInBytes) {
        std::lock_guard<std::mutex> lock(mutex_);
        buffer_.assign(sizeInBytes, 0);
    }

    Span<uint8_t> acquireSpan() {
        mutex_.lock();
        return Span<uint8_t>(buffer_.data(), buffer_.size());
    }

    void releaseSpan() { mutex_.unlock(); }

    uint8_t* data() { return buffer_.data(); }
    const uint8_t* data() const { return buffer_.data(); }

    std::size_t size() const { return buffer_.size(); }
    bool empty() const { return buffer_.empty(); }

  private:
    std::vector<uint8_t> buffer_;
    std::mutex mutex_;
};

}  // namespace core

#endif  // CORE_FRAMEBUFFER_H
