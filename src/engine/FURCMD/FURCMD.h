#pragma once
#include <SDK/archetyped/hash/hash.h>
#include <stdint.h>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include "ankerl/unordered_dense.h"

struct alignas(8) FURCMDPacket {
    uint32_t methodHash;    
    uint16_t payloadSize;   
    uint16_t flags;         
    void* payload;          
    void* outputBuffer;     
    uint64_t* fence = nullptr;        
};

typedef void (*FURMethod) (FURCMDPacket& packet);

struct InternalPacket {
    FURCMDPacket packet;
    std::vector<uint8_t> payloadData;
};

class FURCommandManager {
public:
    FURCommandManager();
    ~FURCommandManager();

    void registerFURMethod(uint32_t hashId, FURMethod method);
    void invoke(FURCMDPacket& packet);
    // Stop the worker thread and drain remaining queue. Must be called BEFORE
    // static destructors of loaded modules run (the worker may otherwise keep
    // dispatching module handlers into already-destroyed state at exit).
    void stop();

private:
    void workerThread();
    void processSinglePacket(InternalPacket& internal);

    ankerl::unordered_dense::map<uint32_t, FURMethod, IdentityHash> methodsTable;
    std::queue<InternalPacket> commandQueue;
    std::mutex queueMutex;
    std::condition_variable cv;
    std::thread worker;
    std::atomic<bool> running;
    std::thread::id workerThreadId;
    
    bool tracingEnabled = false;
};

