#include "FURCMD.h"
#include <assert.h>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <stdexcept>

FURCommandManager::FURCommandManager() : running(true) {
    worker = std::thread(&FURCommandManager::workerThread, this);
}

FURCommandManager::~FURCommandManager() {
    stop();
}

void FURCommandManager::stop() {
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        running = false;
    }
    cv.notify_one();
    if (worker.joinable()) {
        worker.join();
    }
    // Drain any packets still queued so their fences (if any) are released.
    // Handlers themselves are NOT invoked here: module static state may
    // already be gone during shutdown.
    std::queue<InternalPacket> leftover;
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        leftover.swap(commandQueue);
    }
    while (!leftover.empty()) {
        if (leftover.front().packet.fence != nullptr) {
            auto activeFence = std::atomic_ref<uint64_t>(*leftover.front().packet.fence);
            activeFence.store(1, std::memory_order_release);
        }
        leftover.pop();
    }
}

void FURCommandManager::registerFURMethod(uint32_t hashId, FURMethod method) {
    std::lock_guard<std::mutex> lock(queueMutex);
    methodsTable.emplace(hashId, method);
}

void FURCommandManager::invoke(FURCMDPacket &packet) {
    if (std::this_thread::get_id() == workerThreadId) {
        InternalPacket internal;
        internal.packet = packet;
        if (packet.payload && packet.payloadSize > 0) {
            internal.payloadData.resize(packet.payloadSize);
            std::memcpy(internal.payloadData.data(), packet.payload, packet.payloadSize);
            internal.packet.payload = internal.payloadData.data();
        }
        processSinglePacket(internal);
        return;
    }

    InternalPacket internal;
    internal.packet = packet;
    if (packet.payload && packet.payloadSize > 0) {
        internal.payloadData.resize(packet.payloadSize);
        std::memcpy(internal.payloadData.data(), packet.payload,
                    packet.payloadSize);
        internal.packet.payload = internal.payloadData.data();
    }

    {
        std::lock_guard<std::mutex> lock(queueMutex);
        commandQueue.push(std::move(internal));
    }
    cv.notify_one();
}

void FURCommandManager::processSinglePacket(InternalPacket& internal) {
    auto it = methodsTable.find(internal.packet.methodHash);
    if (it != methodsTable.end()) [[likely]] {
        it->second(internal.packet);
    }

    if (internal.packet.fence != nullptr) {
        auto activeFence = std::atomic_ref<uint64_t>(*internal.packet.fence);
        activeFence.store(1, std::memory_order_release);
    }
}

void FURCommandManager::workerThread() {
    workerThreadId = std::this_thread::get_id();
    while (running) {
        InternalPacket internal;
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            cv.wait(lock, [this] { return !commandQueue.empty() || !running; });
            if (!running) break;
            internal = std::move(commandQueue.front());
            commandQueue.pop();
            if (!internal.payloadData.empty()) {
                internal.packet.payload = internal.payloadData.data();
            }
        }

        processSinglePacket(internal);
    }
}
