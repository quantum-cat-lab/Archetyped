#pragma once
#include "alloc_utils.h"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <new>
#include <vector>

/**
 * Arena and Linear allocator in one class; they differ only by the
 * bulk-reclaim trigger:
 *   - ResetMode::PerFrame  : reset() every frame (temporary / scratch data)
 *   - ResetMode::PerModule : reset() only at module unload (module lifetime)
 *
 * allocate() is a lock-free CAS bump. Individual allocations are never
 * freed; memory comes back in bulk via reset(). reset() must not run
 * concurrently with allocate() (frame/unload boundary implies
 * synchronization).
 */
class BumpAllocator {
public:
    enum class ResetMode { PerFrame, PerModule };

    explicit BumpAllocator(ResetMode mode, size_t blockSize = 64 * 1024)
        : m_mode(mode), m_blockCapacity((blockSize + 63) & ~size_t(63)) {
        addBlock();
    }

    ~BumpAllocator() { freeBlocks(); }

    BumpAllocator(const BumpAllocator&) = delete;
    BumpAllocator& operator=(const BumpAllocator&) = delete;

    // On block overflow the failed reservation dies with the old block;
    // wasted tail is bounded by one padded allocation.
    void* allocate(size_t size, size_t alignment = 16) {
        // Padded advances keep every allocation aligned: block bases are
        // 64-aligned and offsets only move in multiples of `alignment`.
        size_t padded = (size + alignment - 1) & ~(alignment - 1);
        for (;;) {
            uint8_t* base = m_currentBlock.load(std::memory_order_acquire);
            size_t prev = m_offset.load(std::memory_order_relaxed);
            if (prev + padded <= m_blockCapacity &&
                m_offset.compare_exchange_weak(prev, prev + padded,
                                               std::memory_order_relaxed,
                                               std::memory_order_relaxed)) {
                // Offset CAS and block pointer must belong to the same
                // epoch; a rotation between the loads kills the reservation.
                if (m_currentBlock.load(std::memory_order_acquire) == base)
                    return base + prev;
                continue;
            }
            std::lock_guard<std::mutex> g(m_growthMtx);
            if (m_offset.load(std::memory_order_acquire) + padded > m_blockCapacity)
                addBlock();
        }
    }

    void reset() {
        std::lock_guard<std::mutex> g(m_growthMtx);
        freeBlocks();
        addBlock();
    }

    ResetMode mode() const { return m_mode; }
    size_t blockCapacity() const { return m_blockCapacity; }
    size_t blockCount() const { return m_blocks.size(); } // approximate under concurrency
    size_t getCurrentOffset() const { return m_offset.load(std::memory_order_relaxed); }

private:
    void addBlock() {
        uint8_t* b = static_cast<uint8_t*>(fh_aligned_alloc(64, m_blockCapacity));
        if (!b) throw std::bad_alloc();
        m_blocks.push_back(b);
        m_currentBlock.store(b, std::memory_order_release);
        m_offset.store(0, std::memory_order_release);
    }

    void freeBlocks() {
        for (uint8_t* b : m_blocks)
            fh_aligned_free(b);
        m_blocks.clear();
        m_currentBlock.store(nullptr, std::memory_order_release);
        m_offset.store(0, std::memory_order_release);
    }

    ResetMode m_mode;
    size_t m_blockCapacity;

    // Cold path only (growth / reset), guarded by m_growthMtx.
    std::vector<uint8_t*> m_blocks;
    std::mutex m_growthMtx;

    std::atomic<uint8_t*> m_currentBlock{nullptr};
    std::atomic<size_t> m_offset{0};
};
