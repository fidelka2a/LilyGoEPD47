#ifndef CORE_EVENT_QUEUE_H
#define CORE_EVENT_QUEUE_H

#include <array>
#include <atomic>

#include "Message.h"

namespace core {

class EventQueue {
  public:
    static constexpr std::size_t Capacity = 4;

    EventQueue() : head_(0), tail_(0) {}

    bool push(const Message& message) {
        auto head = head_.load(std::memory_order_relaxed);
        auto next = increment(head);
        if (next == tail_.load(std::memory_order_acquire)) {
            return false;  // queue full
        }

        buffer_[head] = message;
        head_.store(next, std::memory_order_release);
        return true;
    }

    bool pop(Message& out) {
        auto tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire)) {
            return false;  // queue empty
        }

        out = buffer_[tail];
        tail_.store(increment(tail), std::memory_order_release);
        return true;
    }

  private:
    static constexpr std::size_t increment(std::size_t value) { return (value + 1) % Capacity; }

    std::array<Message, Capacity> buffer_{};
    std::atomic<std::size_t> head_;
    std::atomic<std::size_t> tail_;
};

}  // namespace core

#endif  // CORE_EVENT_QUEUE_H
