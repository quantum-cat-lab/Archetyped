#include "MutexStore.h"
#include <cassert>

MutexStore::MutexStore() {
    chunks.push_back(std::make_unique<MutexChunk>());
    for (uint32_t i = 0; i < kChunkSize; ++i)
        freeList.push_back(i);
}

MutexSlot& MutexStore::slot(uint32_t id) {
    return chunks[id / kChunkSize]->slots[id % kChunkSize];
}

const MutexSlot& MutexStore::slot(uint32_t id) const {
    return chunks[id / kChunkSize]->slots[id % kChunkSize];
}

MutexHandle MutexStore::createMutex() {
    std::lock_guard<std::mutex> g(allocMtx);

    if (freeList.empty()) {
        uint32_t base = static_cast<uint32_t>(chunks.size()) * kChunkSize;
        chunks.push_back(std::make_unique<MutexChunk>());
        for (uint32_t i = 0; i < kChunkSize; ++i)
            freeList.push_back(base + i);
    }

    uint32_t id = freeList.back();
    freeList.pop_back();

    auto& s = slot(id);
    s.gen.fetch_add(1, std::memory_order_relaxed);
    s.refCount.store(0, std::memory_order_relaxed);
    s.attachedBuffer = 0;
    return {id, s.gen.load(std::memory_order_acquire)};
}

void MutexStore::destroyMutex(MutexHandle h) {
    std::lock_guard<std::mutex> g(allocMtx);

    if (h.id >= chunks.size() * kChunkSize) return;
    auto& s = slot(h.id);
    if (s.gen.load(std::memory_order_acquire) != h.gen) return; // stale handle
    if (s.refCount.load(std::memory_order_acquire) != 0) return; // still held

    freeList.push_back(h.id);
}

void MutexStore::lock(MutexHandle h, bool shared) {
    auto& s = slot(h.id); // O(1), no registry lock
    if (s.gen.load(std::memory_order_acquire) != h.gen) return; // reuse-UAF guard

    s.refCount.fetch_add(1, std::memory_order_acq_rel);

    if (shared) s.lock.lock_shared();
    else        s.lock.lock();
}

void MutexStore::unlock(MutexHandle h, bool shared) {
    auto& s = slot(h.id);
    if (s.gen.load(std::memory_order_acquire) != h.gen) return;

    if (shared) s.lock.unlock_shared();
    else        s.lock.unlock();

    s.refCount.fetch_sub(1, std::memory_order_acq_rel);
}

void MutexStore::attachBuffer(MutexHandle h, uint32_t bufferId) {
    std::lock_guard<std::mutex> g(allocMtx);
    if (h.id >= chunks.size() * kChunkSize) return;
    auto& s = slot(h.id);
    if (s.gen.load(std::memory_order_acquire) != h.gen) return;
    s.attachedBuffer = bufferId;
}

uint32_t MutexStore::attachedBuffer(MutexHandle h) const {
    if (h.id >= chunks.size() * kChunkSize) return 0;
    const auto& s = slot(h.id);
    if (s.gen.load(std::memory_order_acquire) != h.gen) return 0;
    return s.attachedBuffer;
}
