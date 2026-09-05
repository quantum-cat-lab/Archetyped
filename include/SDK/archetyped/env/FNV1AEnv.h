#pragma once
#include <stdint.h>
#include <vector>
#include <initializer_list>

#define FNV1A_INVALID_INDEX 0xFFFFFFFFu

struct FNV1aSlot {
    uint32_t hashId;
    uint32_t idx;
    uint32_t gen;
};

struct SlotDesc {
    uint32_t value; // dense idx in host table or hashId (if not cached)
    bool cached;    // true if idx is cached
};

class FNV1AEnv {
public:
    explicit FNV1AEnv(const uint32_t* initialHashes, uint32_t count) {
        slots_.reserve(count);
        for (uint32_t i = 0; i < count; ++i) {
            slots_.push_back(FNV1aSlot{
                .hashId = initialHashes[i],
                .idx = FNV1A_INVALID_INDEX,
                .gen = 0
            });
        }
    }

    FNV1AEnv(std::initializer_list<uint32_t> hashes) {
        slots_.reserve(hashes.size());
        for (uint32_t h : hashes) {
            slots_.push_back(FNV1aSlot{
                .hashId = h,
                .idx = FNV1A_INVALID_INDEX,
                .gen = 0
            });
        }
    }

    inline SlotDesc get(uint32_t slotIdx) const noexcept {
        if (slotIdx >= slots_.size()) {
            return SlotDesc{0, false};
        }
        const auto& slot = slots_[slotIdx];
        if (slot.idx != FNV1A_INVALID_INDEX) {
            return SlotDesc{slot.idx, true};
        } else {
            return SlotDesc{slot.hashId, false};
        }
    }

    inline uint32_t getHash(uint32_t slotIdx) const noexcept {
        return (slotIdx < slots_.size()) ? slots_[slotIdx].hashId : 0;
    }

    inline void cache(uint32_t slotIdx, uint32_t idx, uint32_t gen) noexcept {
        if (slotIdx < slots_.size()) {
            slots_[slotIdx].idx = idx;
            slots_[slotIdx].gen = gen;
        }
    }

    inline void invalidate() noexcept {
        for (auto& slot : slots_) {
            slot.idx = FNV1A_INVALID_INDEX;
            slot.gen = 0;
        }
    }

    uint32_t size() const noexcept { return static_cast<uint32_t>(slots_.size()); }

private:
    std::vector<FNV1aSlot> slots_;
};
