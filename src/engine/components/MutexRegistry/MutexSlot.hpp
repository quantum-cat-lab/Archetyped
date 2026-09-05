#pragma once 
#include <cstdint>
#include <mutex>
#include <shared_mutex>
#include <atomic>

struct alignas(64) MutexSlot {
    std::atomic<uint32_t> gen;
    std::shared_mutex lock;
    uint32_t attachedBuffer;
    std::atomic<uint32_t> refCount;
};
