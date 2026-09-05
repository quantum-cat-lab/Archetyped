#pragma once
#include <atomic>
#include <optional>
#include <vector>
template<typename T, size_t Size = 65536>
class TaskQueue {
    T buffer[Size];
    std::atomic<size_t> head{0};
    std::atomic<size_t> tail{0};
public:
    bool push(const T& task) {
        size_t t = tail.load(std::memory_order_relaxed);
        size_t next_t = (t + 1) % Size;
        if (next_t == head.load(std::memory_order_acquire)) return false;

        buffer[t] = task;
        tail.store(next_t, std::memory_order_release);
        return true;
    }

    std::optional<T> pop() {
        size_t h = head.load(std::memory_order_relaxed);
        while (true) {
            if (h == tail.load(std::memory_order_acquire)) return std::nullopt;

            T task = buffer[h];
            if (head.compare_exchange_strong(h, (h + 1) % Size, std::memory_order_acq_rel)) {
                return task;
            }
        }
    }
    bool empty() const {
        return head.load(std::memory_order_acquire) == tail.load(std::memory_order_acquire);
    }
};