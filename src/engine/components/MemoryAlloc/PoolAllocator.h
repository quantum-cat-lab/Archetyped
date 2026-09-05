#pragma once
#include "alloc_utils.h"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <new>
#include <vector>

/**
 * Fixed-size slot pool with a lock-free tagged free-list. Base primitive:
 * UniversalAllocator builds its bins on top, CommandPagePool/FencePool reuse
 * directly.
 *
 * - Slots live in immutable chunks; growth appends chunks and never moves
 *   existing slots.
 * - Free-list head is a tagged 64-bit atomic: (generation << 32) | index.
 *   Generation bumps on every head change, closing ABA on index reuse.
 * - Push writes node->next BEFORE the head CAS publishes it (Treiber).
 * - acquire()/release() are lock-free; chunk growth takes m_growthMtx
 *   (cold path) but still publishes through the same lock-free CAS.
 *
 * Contract: release() exactly once per acquired slot; contents are
 * caller-managed after acquire().
 */
class PoolAllocator {
public:
    PoolAllocator(size_t slotSize, size_t alignment = 16, size_t chunkSlots = 512)
        : m_slotSize(roundUpAlign(slotSize < sizeof(uint32_t) ? sizeof(uint32_t)
                                                              : slotSize,
                                 alignment)),
          m_chunkSlots(chunkSlots) {
        growChunk();
    }

    PoolAllocator(const PoolAllocator&) = delete;
    PoolAllocator& operator=(const PoolAllocator&) = delete;

    ~PoolAllocator() {
        for (const std::unique_ptr<Chunk>& c : m_chunks)
            fh_aligned_free(c->base);
    }

    void* acquire() {
        uint64_t head = m_head.load(std::memory_order_acquire);
        for (;;) {
            uint32_t idx = uint32_t(head);
            if (idx == kNilIndex) {
                std::lock_guard<std::mutex> g(m_growthMtx);
                if (uint32_t(m_head.load(std::memory_order_relaxed)) == kNilIndex)
                    growChunk();
                head = m_head.load(std::memory_order_acquire);
                continue;
            }
            // Linked before publish: safe to read unconditionally.
            uint32_t next = nextOf(idx).load(std::memory_order_relaxed);
            uint64_t published = (tag(head) << 32) | next;
            if (m_head.compare_exchange_weak(head, published,
                                             std::memory_order_acq_rel,
                                             std::memory_order_acquire))
                return ptr(idx);
        }
    }

    void release(void* p) {
        uint8_t* raw = static_cast<uint8_t*>(p);
        uint64_t head = m_head.load(std::memory_order_acquire);
        for (;;) {
            uint32_t idx = uint32_t(indexOf(raw)); // caller bug if foreign
            nextOf(idx).store(uint32_t(head), std::memory_order_relaxed);
            uint64_t published = (tag(head) << 32) | idx;
            if (m_head.compare_exchange_weak(head, published,
                                             std::memory_order_acq_rel,
                                             std::memory_order_acquire))
                return;
        }
    }

    size_t indexOf(const void* p) const {
        auto raw = static_cast<const uint8_t*>(p);
        for (size_t ci = 0; ci < m_chunks.size(); ++ci) {
            const Chunk& c = *m_chunks[ci];
            if (raw >= c.base && raw < c.base + c.bytes)
                return c.firstIndex + size_t(raw - c.base) / m_slotSize;
        }
        return SIZE_MAX;
    }

    void* ptr(size_t globalIndex) {
        const Chunk& c = *m_chunks[globalIndex / m_chunkSlots];
        return c.base + (globalIndex % m_chunkSlots) * m_slotSize;
    }

    size_t slotSize() const { return m_slotSize; }
    size_t chunkCount() const { return m_chunks.size(); }

private:
    static constexpr uint32_t kNilIndex = 0xFFFFFFFFu;

    struct Chunk {
        uint8_t* base;
        size_t bytes;
        size_t firstIndex;
    };

    std::vector<std::unique_ptr<Chunk>> m_chunks;

    static size_t roundUpAlign(size_t n, size_t a) { return (n + a - 1) & ~(a - 1); }
    static uint64_t tag(uint64_t head) { return (head >> 32) + 1; }

    static size_t alignedBytes(size_t alignment, size_t n) {
        return roundUpAlign(n, alignment);
    }

    std::atomic<uint32_t>& nextOf(uint32_t idx) {
        return *reinterpret_cast<std::atomic<uint32_t>*>(
            static_cast<uint8_t*>(ptr(idx)));
    }

    void growChunk() {
        uint8_t* base = static_cast<uint8_t*>(fh_aligned_alloc(
            64, alignedBytes(64, m_chunkSlots * m_slotSize)));
        if (!base)
            throw std::bad_alloc();
        size_t firstIndex = m_chunks.size() * size_t(m_chunkSlots);

        m_chunks.reserve(m_chunks.size() + 1);
        m_chunks.push_back(std::make_unique<Chunk>(
            Chunk{base, alignedBytes(64, m_chunkSlots * m_slotSize),
                  firstIndex}));

        for (size_t i = 0; i + 1 < m_chunkSlots; ++i)
            nextOf(uint32_t(firstIndex + i))
                .store(uint32_t(firstIndex + i + 1), std::memory_order_relaxed);

        uint32_t first = uint32_t(firstIndex);
        uint32_t last = uint32_t(firstIndex + m_chunkSlots - 1);
        uint64_t head = m_head.load(std::memory_order_acquire);
        for (;;) {
            nextOf(last).store(uint32_t(head), std::memory_order_relaxed);
            uint64_t published = (tag(head) << 32) | first;
            if (m_head.compare_exchange_weak(head, published,
                                             std::memory_order_acq_rel,
                                             std::memory_order_acquire))
                return;
        }
    }

    size_t m_slotSize;
    size_t m_chunkSlots;
    std::atomic<uint64_t> m_head{(uint64_t(1) << 32) | kNilIndex};
    std::mutex m_growthMtx;
};
