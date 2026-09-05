#pragma once
#include "MutexSlot.hpp"
#include <cstdint>
#include <vector>
#include <memory>
#include <mutex>
#include <array>

/**
 * MutexStore - kernel-side mutex registry (FURCMD lock-free redesign).
 *
 * Modules hold a value-type handle (id + gen), never the C++ mutex object.
 * The registry owns the actual mutexes in stable chunks so they survive growth.
 *
 * - createMutex()/destroyMutex() are RARE (module load / buffer alloc) and go
 *   through allocMtx + a free-list of ids.
 * - lock()/unlock() are HOT PATH: O(1) slot lookup with NO registry lock, only
 *   the target slot is touched. Never call allocMtx on the hot path.
 * - gen bumps on every reuse so a stale handle (id from a destroyed+reused
 *   mutex) silently fails instead of locking the wrong buffer (reuse-UAF guard).
 */
struct MutexHandle {
    uint32_t id;
    uint32_t gen;
};

class MutexStore {
public:
    static constexpr uint32_t kChunkSize = 1024;

    MutexStore();
    ~MutexStore() = default;

    /**
     * Allocates a new mutex slot, bumps its generation, returns a handle.
     * Grows the chunk list (never reallocs existing chunks) on exhaustion, so
     * any already-locked shared_mutex stays at a stable address.
     */
    MutexHandle createMutex();

    /**
     * Returns a slot to the free-list. No-op if the handle is stale (gen
     * mismatch) or if refCount != 0 (someone still holds it).
     */
    void destroyMutex(MutexHandle h);

    /**
     * Acquires the mutex for the given handle.
     * HOT PATH: no allocMtx; only the target slot's atomics are touched.
     * refCount is incremented BEFORE the actual lock to avoid a TOCTOU window
     * where destroyMutex could free a slot whose lock() is still in flight.
     * Silently returns on gen mismatch (reuse-UAF guard).
     */
    void lock(MutexHandle h, bool shared);

    /**
     * Releases the mutex for the given handle. Symmetric to lock().
     */
    void unlock(MutexHandle h, bool shared);

    /**
     * Links a kernel buffer to a mutex slot. The link lives ONLY here
     * (attachedBuffer) and in the buffer's own mutexId field; the handle does
     * not store bufferIndex. Caller must ensure no lock is held when linking.
     */
    void attachBuffer(MutexHandle h, uint32_t bufferId);

    uint32_t attachedBuffer(MutexHandle h) const;

private:
    struct MutexChunk {
        std::array<MutexSlot, kChunkSize> slots;
    };

    // Stable chunk storage: growing the vector never moves existing chunks,
    // so a locked shared_mutex inside an old chunk keeps its address.
    std::vector<std::unique_ptr<MutexChunk>> chunks;

    // Free list of available slot ids (under allocMtx only).
    std::vector<uint32_t> freeList;

    // Guards createMutex/destroyMutex and freeList growth ONLY.
    // Must NOT be taken on the lock()/unlock() hot path.
    mutable std::mutex allocMtx;

    MutexSlot& slot(uint32_t id);
    const MutexSlot& slot(uint32_t id) const;
};
