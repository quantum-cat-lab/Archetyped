#pragma once
#include "PoolAllocator.h"
#include "alloc_utils.h"
#include <array>
#include <cstdint>
#include <cstring>
#include <memory>
#include <new>

/**
 * Segregated size-class allocator over PoolAllocator bins. Size classes are
 * powers of two, 16..4096 (bins 0..8); larger requests fall through to
 * direct aligned allocation.
 *
 * Every allocation carries a kHeaderSize header holding the bin id
 * (kDirectBin = direct fallback), making deallocate() O(1). Payload sits
 * after the header; slots are slotSize + kHeaderSize so payload alignment
 * matches the bin class. Watch-layer features (canaries, leak accounting)
 * intentionally excluded - deferred by MemoryController decision.
 */
class UniversalAllocator {
public:
    static constexpr size_t kMinClass = 16;
    static constexpr size_t kMaxClass = 4096;
    static constexpr size_t kNumBins = 9; // 16, 32, ..., 4096
    static constexpr uint32_t kDirectBin = 0xFFFFFFFFu;

    explicit UniversalAllocator(size_t chunkSlotsPerBin = 512) {
        for (size_t s = kMinClass, i = 0; i < kNumBins; s *= 2, ++i)
            m_bins[i] = std::make_unique<PoolAllocator>(s, s, chunkSlotsPerBin);
    }

    UniversalAllocator(const UniversalAllocator&) = delete;
    UniversalAllocator& operator=(const UniversalAllocator&) = delete;

    void* allocate(size_t size) {
        if (size == 0)
            size = 1;
        int bin = sizeClassIndex(size + kHeaderSize);
        if (bin < 0) {
            size_t total = ((roundUp16(size) + kHeaderSize + 63) / 64) * 64;
            uint8_t* raw = static_cast<uint8_t*>(fh_aligned_alloc(64, total));
            if (!raw)
                throw std::bad_alloc();
            storeTag(raw, kDirectBin);
            return raw + kHeaderSize;
        }
        PoolAllocator& pool = *m_bins[size_t(bin)];
        uint8_t* raw = static_cast<uint8_t*>(pool.acquire());
        storeTag(raw, uint32_t(bin));
        return raw + kHeaderSize;
    }

    void deallocate(void* p) {
        if (!p)
            return;
        uint8_t* raw = static_cast<uint8_t*>(p) - kHeaderSize;
        uint32_t tag = loadTag(raw);
        if (tag == kDirectBin) {
            fh_aligned_free(raw);
            return;
        }
        m_bins[tag]->release(raw);
    }

    static int sizeClassIndex(size_t effective) {
        if (effective > kMaxClass)
            return -1;
        size_t s = effective - 1;
        int log = 0;
        while (s >>= 1)
            ++log;          // floor(log2(effective-1)) == ceil(log2(effective))-1
        return log - 3;     // 2^4=16 is bin 0
    }

private:
    static constexpr size_t kHeaderSize = sizeof(uint64_t);

    static size_t roundUp16(size_t n) { return (n + 15) & ~size_t(15); }
    static void storeTag(uint8_t* raw, uint32_t tag) {
        std::memcpy(raw, &tag, sizeof(tag));
    }
    static uint32_t loadTag(const uint8_t* raw) {
        uint32_t tag;
        std::memcpy(&tag, raw, sizeof(tag));
        return tag;
    }

    std::array<std::unique_ptr<PoolAllocator>, kNumBins> m_bins;
};
