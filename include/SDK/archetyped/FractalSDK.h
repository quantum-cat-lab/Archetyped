#pragma once
#include <SDK/archetyped/hash/hash.h>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <utility>
#include <vector>
#include <atomic>
#include <memory>
#include <iostream>
#include <string>
#include <cstring>
#include <filesystem>
#include <SDK/archetyped/IKernel.h>

// --- Kernel CMDs - fast kernel syscalls  ---

struct KernelCMDPacket
{
    uint8_t cmdId;
    void *in;
    void *out;
};

using KernelCMD = void (*) (KernelCMDPacket&) noexcept;

// --- FUR CMD PACKET - FAST UTILITY ROUTER COMMAND PACKET ---

struct FURCMDPacket {
    uint32_t methodHash;    // ID of the method being invoked
    uint16_t payloadSize;   // Size of input data (up to 64 KB)
    uint16_t flags;         // System flags (e.g., priority or call type)

    void* payload;          // Pointer to input data (arguments)

    void* outputBuffer;     // Pointer to buffer where the engine will write the response

    uint64_t* fence = nullptr;      // Pointer to an atomic synchronization ticket
};
typedef void (*FURMethod) (FURCMDPacket& packet);


// --- ECS Contexts ---

struct Entity {
    uint32_t id;
    uint32_t version{0};

    bool operator==(const Entity& other) const {
        return id == other.id && version == other.version;
    }
};

namespace std {
    template<>
    struct hash<Entity> {
        size_t operator()(const Entity& e) const {
            return hash<uint32_t>()(e.id) ^ (hash<uint32_t>()(e.version) << 1);
        }
    };
}

struct attachComponentDeferredInstanceCMDContext {
    uint32_t domainId;
    Entity entity;
    uint32_t componentId;
    void* componentData;
    uint32_t dataSize;
};

struct removeComponentDeferredInstanceCMDContext {
    uint32_t domainId;
    Entity entity;
    uint32_t componentId;
};

struct registerComponentInstanceCMDContext {
    uint32_t domainId;
    uint32_t componentId;
    uint32_t componentSize;
    uint32_t capacity;
};

struct resizeComponentCMDContext {
    uint32_t domainId;
    uint32_t componentId;
    uint32_t newCapacity;
};

struct flushCommandsInstanceCMDContext {
    uint32_t domainId;
};

struct getComponentInstanceCMDContext {
    uint32_t domainId;
    Entity entity;
    uint32_t componentId;
};

struct hasComponentInstanceCMDContext {
    uint32_t domainId;
    Entity entity;
    uint32_t componentId;
};

struct registerGroupInstanceCMDContext {
    uint32_t domainId;
    uint32_t* componentHashIds;
    uint32_t count;
};

struct containsInstanceCMDContext {
    uint32_t domainId;
    uint32_t* componentHashIds;
    uint32_t count;
    uint32_t value;
};

struct hasAllInstanceCMDContext {
    uint32_t domainId;
    Entity entity;
    uint32_t* componentHashIds;
    uint32_t count;
};

struct queryEntitiesInstanceCMDContext {
    uint32_t domainId;
    uint32_t* componentHashIds;
    uint32_t count;
};

struct getGroupSizeInstanceCMDContext {
    uint32_t domainId;
    uint32_t* componentHashIds;
    uint32_t count;
};

struct getRawPtrInstanceCMDContext {
    uint32_t domainId;
    uint32_t componentId;
    bool lock;
};

struct getDensePtrInstanceCMDContext {
    uint32_t domainId;
    uint32_t componentId;
};

struct onComponentAttachedInstanceCMDContext {
    uint32_t domainId;
    Entity entity;
    uint32_t componentId;
};

struct onComponentRemovedInstanceCMDContext {
    uint32_t domainId;
    Entity entity;
    uint32_t componentId;
};

struct setComponentLockCMDContext {
    uint32_t domainId;
    uint32_t componentId;
    bool lock;
};

struct getComponentLockCMDContext {
    uint32_t domainId;
    uint32_t componentId;
};

struct destroyEntityCMDContext {
    uint32_t domainId;
    Entity entity;
};

struct getDenseIndexPtrCMDContext {
    uint32_t domainId;
    uint32_t componentId;
};

// --- ECS Registry/Vault Contexts ---

struct createRegistryCMDContext {
    uint32_t registryId;
};

struct registerTypeCMDContext {
    uint32_t registryId;
    uint32_t componentId;
    uint32_t componentSize;
    uint32_t alignment;
};

struct createVaultCMDContext {
    uint32_t vaultId;
    uint32_t registryId; // 0xFFFFFFFF = no registry
};

struct bindRegistryToVaultCMDContext {
    uint32_t vaultId;
    uint32_t registryId;
};

struct registerDomainWithVaultCMDContext {
    uint32_t domainId;
    uint32_t vaultId;
};

struct registerTableFromRegistryCMDContext {
    uint32_t domainId;
    uint32_t componentId;
    uint32_t capacity;
};

struct destroyDomainCMDContext {
    uint32_t domainId;
    uint32_t vaultId;
};

struct linkComponentTableCMDContext {
    uint32_t targetDomainId;
    uint32_t componentId;
    uint32_t sourceDomainId;
};

// -- Work Scheduler Contexts ---
typedef void (*w_task_fn)(void* context);
struct ComputeTask {
    w_task_fn fn;
    void* context;
};

// --- SQL Database Contexts ---

struct registerSQLDomainContext {
    uint32_t domainId;

    const char* domainName;
};
struct openInstanceCMDContext {

    uint32_t domainId;

    const char* dbPath;

};
struct executeInstanceCMDContext {
    uint32_t domainId;

    const char* sql;
};
struct setStringInstanceCMDContext {
    uint32_t domainId;

    const char* key;

    const char* value;
};
struct getStringInstanceCMDContext {
    uint32_t domainId;

    const char* key;

    uint32_t bufferSize;

};
struct existsInstanceCMDContext {
    uint32_t domainId;

    const char* key;

};
struct closeInstanceCMDContext {
    uint32_t domainId;

};
struct isOpenInstanceCMDContext {
    uint32_t domainId;

};


// --- Event System Contexts ---

struct EventData {
    void* ptr;
    size_t size;
}; //Event Data Context


typedef void (*EventCallback)(uint32_t, const EventData&, void*);

struct Subscriber {
    EventCallback cb;
    void* user;
    uint32_t subscriberId;
};

struct QueuedEvent {
    uint32_t id;
    EventData data;
};

struct subscribeEventInstanceCMDContext {
    uint32_t domainId;
    uint32_t eventId;
    EventCallback cb;
    void* user;
    uint32_t subscriberId;
};
struct emitEventInstanceCMDContext {
    uint32_t domainId;
    uint32_t eventId;
    EventData data;
};
struct pushEventInstanceCMDContext {
    uint32_t domainId;
    uint32_t eventId;
    EventData data;
};
struct processEventsInstanceCMDContext {
    uint32_t domainId;
};
struct resetEventsInstanceCMDContext {
    uint32_t domainId;
};
struct unsubscribeEventInstanceCMDContext {
    uint32_t domainId;
    uint32_t subscriberId;
};
// --- Module Loader Contexts ---

struct loadModuleContext {
    const char* path;
};
struct isLoadedCMDContext {
    const char* path;
};
struct setModuleStateContext {
    const char* path;
    int state;
};
struct getModuleStateContext {
    const char* path;
};

// --- VFS Contexts ---
struct vfsResolveCMDContext {
    const char* virtualPath;
};

struct vfsSetRootCMDContext {
    char path[512];
};

struct vfsMountCMDContext {
    char scheme[64];
    char root[1024];
    int32_t priority;
    uint8_t writable;
};

struct vfsUnmountCMDContext {
    char scheme[64];
};

// --- Clock Contexts ---

struct getDeltaTimeContext {};
struct getTotalTimeContext {};
struct getTimeContext {};

// -- Smart Scheduler Contexts ---
typedef void (*ss_task_fn)(void* context);
struct scheduleSSCMDContext {
    ss_task_fn fn;
    void* context;
    uint32_t value; // interval or delay
};

// -- System Execution Contexts ---
typedef void (*system_update_fn)(void* context);
struct registerSystemCMDContext {
    system_update_fn fn;
    void* context;
    uint32_t frequencyMs;
};

struct KernelAPI {
    void (*sendCMDPacket)(void* packetPtr);
    void (*registerCMDMethod)(uint32_t hashId, void* methodPtr);
};
typedef void (*ModuleEntry)(KernelAPI api);


// METHOD HASHES

// --- ECS ---
constexpr uint32_t registerCMDomainHash = fnv1aHashConst("archetyped:ecs:registerCMDomain");
constexpr uint32_t flushCommandsHash = fnv1aHashConst("archetyped:ecs:flushCommands");
constexpr uint32_t registerComponentHash = fnv1aHashConst("archetyped:ecs:registerComponent");
constexpr uint32_t attachComponentDeferredHash = fnv1aHashConst("archetyped:ecs:attachComponentDeferred");
constexpr uint32_t removeComponentDeferredHash = fnv1aHashConst("archetyped:ecs:removeComponentDeferred");
constexpr uint32_t getComponentHash = fnv1aHashConst("archetyped:ecs:getComponent");
constexpr uint32_t hasComponentHash = fnv1aHashConst("archetyped:ecs:hasComponent");
constexpr uint32_t onComponentAttachedHash = fnv1aHashConst("archetyped:ecs:onComponentAttached");
constexpr uint32_t onComponentRemovedHash = fnv1aHashConst("archetyped:ecs:onComponentRemoved");
constexpr uint32_t registerGroupHash = fnv1aHashConst("archetyped:ecs:registerGroup");
constexpr uint32_t containsHash = fnv1aHashConst("archetyped:ecs:contains");
constexpr uint32_t hasAllHash = fnv1aHashConst("archetyped:ecs:hasAll");
constexpr uint32_t getGroupSizeHash = fnv1aHashConst("archetyped:ecs:getGroupSize");
constexpr uint32_t queryEntitiesHash = fnv1aHashConst("archetyped:ecs:queryEntities");
constexpr uint32_t getRawPtrHash = fnv1aHashConst("archetyped:ecs:getRawPtr");
constexpr uint32_t getDensePtrHash = fnv1aHashConst("archetyped:ecs:getDensePtr");
constexpr uint32_t getComponentLockHash = fnv1aHashConst("archetyped:ecs:getComponentLock");
constexpr uint32_t setComponentLockHash = fnv1aHashConst("archetyped:ecs:setComponentLock");
constexpr uint32_t resizeComponentHash = fnv1aHashConst("archetyped:ecs:resizeComponent");
constexpr uint32_t destroyEntityHash = fnv1aHashConst("archetyped:ecs:destroyEntity");
constexpr uint32_t getDenseIndexPtrHash = fnv1aHashConst("archetyped:ecs:getDenseIndexPtr");

// --- ECS Registry/Vault ---
constexpr uint32_t createRegistryHash = fnv1aHashConst("archetyped:ecs:createRegistry");
constexpr uint32_t registerTypeHash = fnv1aHashConst("archetyped:ecs:registerType");
constexpr uint32_t createVaultHash = fnv1aHashConst("archetyped:ecs:createVault");
constexpr uint32_t bindRegistryToVaultHash = fnv1aHashConst("archetyped:ecs:bindRegistryToVault");
constexpr uint32_t registerDomainWithVaultHash = fnv1aHashConst("archetyped:ecs:registerDomainWithVault");
constexpr uint32_t registerTableFromRegistryHash = fnv1aHashConst("archetyped:ecs:registerTableFromRegistry");
constexpr uint32_t linkComponentTableHash = fnv1aHashConst("archetyped:ecs:linkComponentTable");
constexpr uint32_t destroyDomainHash = fnv1aHashConst("archetyped:ecs:destroyDomain");

// --- Smart Scheduler ---
constexpr uint32_t scheduleCyclicHash = fnv1aHashConst("archetyped:smart_scheduler:scheduleCyclic");
constexpr uint32_t scheduleDelayedHash = fnv1aHashConst("archetyped:smart_scheduler:scheduleDelayed");
constexpr uint32_t scheduleTickHash = fnv1aHashConst("archetyped:smart_scheduler:scheduleTick");

// --- System Execution ---
constexpr uint32_t registerSystemHash = fnv1aHashConst("archetyped:system_execution:registerSystem");

// --- Work Scheduler ---

constexpr uint32_t registerWSDomainHash = fnv1aHashConst("archetyped:work_scheduler:registerWSDomain");
constexpr uint32_t scheduleTaskHash = fnv1aHashConst("archetyped:work_scheduler:scheduleTask");

// --- SQL Database ---

constexpr uint32_t registerSQLDomainHash = fnv1aHashConst("archetyped:sqldb:registerSQLDomain");
constexpr uint32_t openCMDHash = fnv1aHashConst("archetyped:sqldb:openCMD");
constexpr uint32_t executeCMDHash = fnv1aHashConst("archetyped:sqldb:executeCMD");
constexpr uint32_t setStringCMDHash = fnv1aHashConst("archetyped:sqldb:setStringCMD");
constexpr uint32_t getStringCMDHash = fnv1aHashConst("archetyped:sqldb:getStringCMD");
constexpr uint32_t existsCMDHash = fnv1aHashConst("archetyped:sqldb:existsCMD");
constexpr uint32_t closeCMDHash = fnv1aHashConst("archetyped:sqldb:closeCMD");
constexpr uint32_t isOpenCMDHash = fnv1aHashConst("archetyped:sqldb:isOpenCMD");

// --- Event System ---

constexpr uint32_t registerEBHash = fnv1aHashConst("archetyped:event_bus:registerEBDomain");
constexpr uint32_t subscribeEventHash = fnv1aHashConst("archetyped:event_bus:subscribeEvent");
constexpr uint32_t emitEventHash = fnv1aHashConst("archetyped:event_bus:emitEvent");
constexpr uint32_t pushEventHash = fnv1aHashConst("archetyped:event_bus:pushEvent");
constexpr uint32_t processEventsHash = fnv1aHashConst("archetyped:event_bus:processEvents");
constexpr uint32_t resetEventsHash = fnv1aHashConst("archetyped:event_bus:resetEvents");
constexpr uint32_t unsubscribeEventHash = fnv1aHashConst("archetyped:event_bus:unsubscribeEvent");

// --- Module Loader ---

constexpr uint32_t loadModuleHash = fnv1aHashConst("archetyped:module_loader:loadModule");
constexpr uint32_t isLoadedHash = fnv1aHashConst("archetyped:module_loader:isLoaded");
constexpr uint32_t setModuleStateHash = fnv1aHashConst("archetyped:module_loader:setState");
constexpr uint32_t getModuleStateHash = fnv1aHashConst("archetyped:module_loader:getState");

// --- Clock ---

constexpr uint32_t getDeltaTimeHash = fnv1aHashConst("archetyped:clock:getDeltaTime");
constexpr uint32_t getTotalTimeHash = fnv1aHashConst("archetyped:clock:getTotalTime");
constexpr uint32_t getTimeHash      = fnv1aHashConst("archetyped:clock:getTime");

// --- VFS ---
constexpr uint32_t vfsResolveHash = fnv1aHashConst("archetyped:vfs:resolve");
constexpr uint32_t vfsSetContainerRootHash = fnv1aHashConst("archetyped:vfs:setContainerRoot");
constexpr uint32_t vfsSetUserRootHash = fnv1aHashConst("archetyped:vfs:setUserRoot");
constexpr uint32_t vfsMountHash = fnv1aHashConst("archetyped:vfs:mount");
constexpr uint32_t vfsUnmountHash = fnv1aHashConst("archetyped:vfs:unmount");

// --- Frame tick (emitted each main-loop iteration, no return) ---
struct FrameTickCMDContext {
    float dt;
    double gameTime;
};
constexpr uint32_t frameTickHash = fnv1aHashConst("archetyped:frame:tick");

// --- Fractal SDK Header ---
// Ticket Pool

struct Ticket {
    std::atomic<uint64_t> fence{0};
    uint32_t id = 0;

    void reset() { fence.store(0, std::memory_order_release); }
    bool isReady() const { return fence.load(std::memory_order_acquire) == 1; }
};

class TicketPool {
public:
    TicketPool(size_t size = 1024) : m_tickets(size) {
        for(uint32_t i = 0; i < size; ++i) m_tickets[i].id = i;
    }

    Ticket* acquire() {
        uint32_t index = m_next.fetch_add(1) % m_tickets.size();
        Ticket* t = &m_tickets[index];
        t->reset(); 
        return t;
    }

private:
    std::vector<Ticket> m_tickets;
    std::atomic<uint32_t> m_next{0};
};


namespace FractalSDK {

class SDK {
public:
    SDK(const SDK&) = delete;
    SDK& operator=(const SDK&) = delete;

    static void Initialize(IKernel* kernel) {
        if (!instance) {
            instance = new SDK(kernel);
        }
    }

    static void Shutdown() {
        if (instance) {
            delete instance;
            instance = nullptr;
        }
    }

    Ticket* allocateTicket() {
        return m_ticketPool->acquire();
    }

    static SDK* Get() { return instance; }

    void sendPacket(FURCMDPacket& packet) {
        if (!instance || !m_kernel) return;
        m_kernel->sendCMDPacket(packet);
    }

    // Register a FUR command handler by method hash (module-side convenience
    // wrapper around IKernel::registerCMDMethod).
    void registerMethod(uint32_t hashId, FURMethod method) {
        if (m_kernel) m_kernel->registerCMDMethod(hashId, method);
    }

    static void SetLoadedModulePath(const char* path) {
        std::strncpy(s_loadedModulePath, path, sizeof(s_loadedModulePath) - 1);
    }

    static const char* getLoadedModulePath() { return s_loadedModulePath; }

private:
    explicit SDK(IKernel* kernel) : m_kernel(kernel) {
        m_ticketPool = std::make_unique<TicketPool>(1024);
    }
    
    ~SDK() = default;

    std::unique_ptr<TicketPool> m_ticketPool;
    IKernel* m_kernel; 
    static inline SDK* instance = nullptr;
    static inline char s_loadedModulePath[512] = "";
};

namespace WorkScheduler {

    inline void scheduleTask(w_task_fn fn, void* context, uint32_t domainId = 0) {
        ComputeTask task;
        task.fn = fn;
        task.context = context;

        uint32_t payloadSize = sizeof(task) + sizeof(domainId);
        std::vector<uint8_t> payload(payloadSize);
        memcpy(payload.data(), &domainId, sizeof(domainId));
        memcpy(payload.data() + sizeof(domainId), &task, sizeof(task));

        FURCMDPacket packet;
        packet.methodHash = scheduleTaskHash;
        packet.payloadSize = payloadSize;
        packet.payload = payload.data();

        SDK::Get()->sendPacket(packet);
    }
}

namespace VFS {
    inline std::string resolve(const char* virtualPath) {
        vfsResolveCMDContext ctx;
        ctx.virtualPath = virtualPath;

        char outBuffer[1024]; // VFS::MAX_PATH_LENGTH
        
        FURCMDPacket packet;
        packet.methodHash = vfsResolveHash;
        packet.payloadSize = sizeof(ctx);
        packet.payload = &ctx;
        packet.outputBuffer = outBuffer;

        SDK::Get()->sendPacket(packet);

        return std::string(outBuffer);
    }

    inline bool fileExists(const char* virtualPath) {
        std::string absPath = resolve(virtualPath);
        if (absPath.empty()) return false;
        return std::filesystem::exists(absPath);
    }

    inline void setContainerRoot(const char* path) {
        vfsSetRootCMDContext ctx;
        std::strncpy(ctx.path, path, sizeof(ctx.path) - 1);
        ctx.path[sizeof(ctx.path) - 1] = '\0';

        FURCMDPacket packet;
        packet.methodHash = vfsSetContainerRootHash;
        packet.payloadSize = sizeof(ctx);
        packet.payload = &ctx;

        SDK::Get()->sendPacket(packet);
    }

    inline void setUserRoot(const char* path) {
        vfsSetRootCMDContext ctx;
        std::strncpy(ctx.path, path, sizeof(ctx.path) - 1);
        ctx.path[sizeof(ctx.path) - 1] = '\0';

        FURCMDPacket packet;
        packet.methodHash = vfsSetUserRootHash;
        packet.payloadSize = sizeof(ctx);
        packet.payload = &ctx;

        SDK::Get()->sendPacket(packet);
    }
}


}
