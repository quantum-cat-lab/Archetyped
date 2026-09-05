#pragma once
#include "FractalSDK.h"
#include "IKernel.h"
#include "VaultInstance.h"
#include <SDK/archetyped/hash/hash.h>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
#include <atomic>

class ECS {
public:
    // Own domain (backward compat)
    ECS(std::string name) {
        CMDomainId = fnv1aHash(name);
        CMDomainName = name;

        Ticket* ticket = FractalSDK::SDK::Get()->allocateTicket();
        FURCMDPacket packet;
        packet.methodHash = registerCMDomainHash;
        packet.payloadSize = sizeof(uint32_t);
        packet.payload = &CMDomainId;
        packet.fence = (uint64_t*)&ticket->fence;

        FractalSDK::SDK::Get()->sendPacket(packet);
        while(!ticket->isReady()) {}
    }

    // Domain from vault (shared storage)
    ECS(std::string name, VaultInstance& vault) {
        CMDomainId = vault.getDomainId();
        CMDomainName = name;
    }

    template<typename T>
    void registerComponent(uint32_t hashId, uint32_t capacity) {
        Ticket* ticket = FractalSDK::SDK::Get()->allocateTicket();
        registerComponentInstanceCMDContext context;
        context.domainId = CMDomainId;
        context.componentId = hashId;
        context.componentSize = sizeof(T);
        context.capacity = capacity;

        FURCMDPacket packet;
        packet.methodHash = registerComponentHash;
        packet.payloadSize = sizeof(context);
        packet.payload = &context;
        packet.fence = (uint64_t*)&ticket->fence;

        FractalSDK::SDK::Get()->sendPacket(packet);
        while(!ticket->isReady()) {}
    }

    void bindTable(uint32_t componentId, uint32_t sourceDomainId) {
        Ticket* ticket = FractalSDK::SDK::Get()->allocateTicket();
        linkComponentTableCMDContext context{ CMDomainId, componentId, sourceDomainId };
        FURCMDPacket packet;
        packet.methodHash = linkComponentTableHash;
        packet.payloadSize = sizeof(context);
        packet.payload = &context;
        packet.fence = (uint64_t*)&ticket->fence;
        FractalSDK::SDK::Get()->sendPacket(packet);
        while (!ticket->isReady()) {}
    }

    void registerComponentFromRegistry(uint32_t hashId, uint32_t capacity) {
        Ticket* ticket = FractalSDK::SDK::Get()->allocateTicket();
        registerTableFromRegistryCMDContext context{ CMDomainId, hashId, capacity };
        FURCMDPacket packet;
        packet.methodHash = registerTableFromRegistryHash;
        packet.payloadSize = sizeof(context);
        packet.payload = &context;
        packet.fence = (uint64_t*)&ticket->fence;
        FractalSDK::SDK::Get()->sendPacket(packet);
        while(!ticket->isReady()) {}
    }

    template<typename T> 
    void attachComponentDeferred(Entity entity, uint32_t componentHashId, T* componentData) {
        attachComponentDeferredInstanceCMDContext context;
        context.domainId = CMDomainId;
        context.entity = entity;
        context.componentId = componentHashId;
        context.componentData = componentData;
        context.dataSize = sizeof(T);

        FURCMDPacket packet;
        packet.methodHash = attachComponentDeferredHash;
        packet.payloadSize = sizeof(context);
        packet.payload = &context;

        FractalSDK::SDK::Get()->sendPacket(packet);
    }

    template<typename T>
    void removeComponentDeferred(Entity entity, uint32_t componentHashId) {
        removeComponentDeferredInstanceCMDContext context;
        context.domainId = CMDomainId;
        context.entity = entity;
        context.componentId = componentHashId;

        FURCMDPacket packet;
        packet.methodHash = removeComponentDeferredHash;
        packet.payloadSize = sizeof(context);
        packet.payload = &context;

        FractalSDK::SDK::Get()->sendPacket(packet);
    }

    void flushCommands() {
        Ticket* ticket = FractalSDK::SDK::Get()->allocateTicket();
        flushCommandsInstanceCMDContext context;
        context.domainId = CMDomainId;

        FURCMDPacket packet;
        packet.methodHash = flushCommandsHash;
        packet.payload = &context;
        packet.payloadSize = sizeof(context);
        packet.fence = (uint64_t*)&ticket->fence;

        FractalSDK::SDK::Get()->sendPacket(packet);
        while(!ticket->isReady()) {}
    }

    void destroyEntity(Entity entity) {
        destroyEntityCMDContext context;
        context.domainId = CMDomainId;
        context.entity = entity;

        FURCMDPacket packet;
        packet.methodHash = destroyEntityHash;
        packet.payloadSize = sizeof(context);
        packet.payload = &context;

        FractalSDK::SDK::Get()->sendPacket(packet);
    }

    template<typename T>
    T* getComponent(Entity entity, uint32_t componentHashId) {
        Ticket* ticket = FractalSDK::SDK::Get()->allocateTicket();
        getComponentInstanceCMDContext context;
        context.domainId = CMDomainId;
        context.entity = entity;
        context.componentId = componentHashId;
        
        void* output = nullptr;
        FURCMDPacket packet;
        packet.methodHash = getComponentHash;
        packet.payloadSize = sizeof(context);
        packet.payload = &context;
        packet.outputBuffer = &output;
        packet.fence = (uint64_t*)&ticket->fence;

        FractalSDK::SDK::Get()->sendPacket(packet);
        while(!ticket->isReady()) {}
        return static_cast<T*>(output);
    }

    bool hasComponent(Entity entity, uint32_t componentHashId) {
        Ticket* ticket = FractalSDK::SDK::Get()->allocateTicket();
        hasComponentInstanceCMDContext context;
        context.domainId = CMDomainId;
        context.entity = entity;
        context.componentId = componentHashId;
        
        bool exists = false;
        FURCMDPacket packet;
        packet.methodHash = hasComponentHash;
        packet.payloadSize = sizeof(context);
        packet.payload = &context;
        packet.outputBuffer = &exists;
        packet.fence = (uint64_t*)&ticket->fence;

        FractalSDK::SDK::Get()->sendPacket(packet);
        while(!ticket->isReady()) {}
        return exists;
    }

    void* getRawPtr(uint32_t componentId, bool lock = false, uint32_t domainId = 0) {
        Ticket* ticket = FractalSDK::SDK::Get()->allocateTicket();
        getRawPtrInstanceCMDContext context;
        context.componentId = componentId;
        context.domainId = (domainId == 0) ? CMDomainId : domainId;
        context.lock = lock;
        
        void* rawPtr = nullptr;
        FURCMDPacket packet;
        packet.methodHash = getRawPtrHash;
        packet.payload = &context;
        packet.payloadSize = sizeof(getRawPtrInstanceCMDContext);
        packet.outputBuffer = &rawPtr;
        packet.fence = (uint64_t*)&ticket->fence;
        
        FractalSDK::SDK::Get()->sendPacket(packet);
        while(!ticket->isReady()) {}
        return rawPtr;
    }

    uint32_t* getDensePtr(uint32_t componentId, uint32_t domainId = 0) {
        Ticket* ticket = FractalSDK::SDK::Get()->allocateTicket();
        getDensePtrInstanceCMDContext context;
        context.componentId = componentId;
        context.domainId = (domainId == 0) ? CMDomainId : domainId;
        
        uint32_t* densePtr = nullptr;
        FURCMDPacket packet;
        packet.methodHash = getDensePtrHash;
        packet.payload = &context;
        packet.payloadSize = sizeof(getDensePtrInstanceCMDContext);
        packet.outputBuffer = &densePtr;
        packet.fence = (uint64_t*)&ticket->fence;
        
        FractalSDK::SDK::Get()->sendPacket(packet);
        while(!ticket->isReady()) {}
        return densePtr;
    }

    void registerGroup(std::initializer_list<uint32_t> hashes) {
        Ticket* ticket = FractalSDK::SDK::Get()->allocateTicket();
        registerGroupInstanceCMDContext context;
        context.componentHashIds = const_cast<uint32_t*>(hashes.begin());
        context.count = (uint32_t)hashes.size();
        context.domainId = CMDomainId;

        FURCMDPacket packet;
        packet.methodHash = registerGroupHash;
        packet.payload = &context;
        packet.payloadSize = sizeof(registerGroupInstanceCMDContext);
        packet.fence = (uint64_t*)&ticket->fence;

        FractalSDK::SDK::Get()->sendPacket(packet);
        while(!ticket->isReady()) {}
    }

    uint32_t getGroupSize(std::initializer_list<uint32_t> hashes) {

        Ticket* ticket = FractalSDK::SDK::Get()->allocateTicket();
        getGroupSizeInstanceCMDContext context;
        context.componentHashIds = const_cast<uint32_t*>(hashes.begin());
        context.count = (uint32_t)hashes.size();
        context.domainId = CMDomainId;

        uint32_t size = 0;
        FURCMDPacket packet;
        packet.methodHash = getGroupSizeHash;
        packet.payload = &context;
        packet.payloadSize = sizeof(getGroupSizeInstanceCMDContext);
        packet.outputBuffer = &size;
        packet.fence = (uint64_t*)&ticket->fence;

        FractalSDK::SDK::Get()->sendPacket(packet);
        while(!ticket->isReady()) {}
        return size;
    }

    std::vector<Entity> query(std::initializer_list<uint32_t> hashes) {
        uint32_t size = getGroupSize(hashes);
        std::vector<Entity> entities(size);

        if (size == 0) return entities;

        Ticket* ticket = FractalSDK::SDK::Get()->allocateTicket();
        queryEntitiesInstanceCMDContext context;
        context.domainId = CMDomainId;
        context.componentHashIds = const_cast<uint32_t*>(hashes.begin());
        context.count = (uint32_t)hashes.size();

        FURCMDPacket packet;
        packet.methodHash = queryEntitiesHash;
        packet.payloadSize = sizeof(context);
        packet.payload = &context;
        packet.outputBuffer = entities.data();
        packet.fence = (uint64_t*)&ticket->fence;

        FractalSDK::SDK::Get()->sendPacket(packet);
        while(!ticket->isReady()) {}
        return entities;
    }

    bool contains(std::initializer_list<uint32_t> array, uint32_t value) {
        Ticket* ticket = FractalSDK::SDK::Get()->allocateTicket();
        containsInstanceCMDContext context;
        context.domainId = CMDomainId;
        context.componentHashIds = const_cast<uint32_t*>(array.begin());
        context.value = value;
        context.count = (uint32_t)array.size();
        
        bool result = false;
        FURCMDPacket packet;
        packet.methodHash = containsHash;
        packet.outputBuffer = &result;
        packet.payload = &context;
        packet.payloadSize = sizeof(context);
        packet.fence = (uint64_t*)&ticket->fence;

        FractalSDK::SDK::Get()->sendPacket(packet);
        while(!ticket->isReady()) {}
        return result;
    }

    Ticket* getComponentAsync(Entity entity, uint32_t componentHashId, void* outputBuffer) {
        Ticket* ticket = FractalSDK::SDK::Get()->allocateTicket();
        auto* taskCtx = new GetComponentTask(); 
        taskCtx->context.entity = entity;
        taskCtx->context.componentId = componentHashId;
        taskCtx->context.domainId = CMDomainId;
        taskCtx->ticket = ticket;
        taskCtx->targetBuffer = static_cast<void**>(outputBuffer);

        auto fn = [](void* ctx) {
            auto* data = static_cast<GetComponentTask*>(ctx);
            FURCMDPacket packet;
            packet.methodHash = getComponentHash;
            packet.payload = &data->context;
            packet.payloadSize = sizeof(getComponentInstanceCMDContext);
            packet.outputBuffer = data->targetBuffer;
            packet.fence = (uint64_t*)&data->ticket->fence;
            FractalSDK::SDK::Get()->sendPacket(packet);
            delete data;
        };
        FractalSDK::WorkScheduler::scheduleTask(fn, taskCtx, 0);
        return ticket;
    }

    Ticket* hasComponentAsync(Entity entity, uint32_t componentHashId, void* outputBuffer) {
        Ticket* ticket = FractalSDK::SDK::Get()->allocateTicket();
        auto* taskCtx = new hasComponentTask(); 
        taskCtx->context.entity = entity;
        taskCtx->context.componentId = componentHashId;
        taskCtx->context.domainId = CMDomainId;
        taskCtx->ticket = ticket;
        taskCtx->targetBuffer = outputBuffer;

        auto fn = [](void* ctx) {
            auto* data = static_cast<hasComponentTask*>(ctx);
            FURCMDPacket packet;
            packet.methodHash = hasComponentHash;
            packet.payload = &data->context;
            packet.payloadSize = sizeof(hasComponentInstanceCMDContext);
            packet.outputBuffer = data->targetBuffer;
            packet.fence = (uint64_t*)&data->ticket->fence;
            FractalSDK::SDK::Get()->sendPacket(packet);
            delete data;
        };
        FractalSDK::WorkScheduler::scheduleTask(fn, taskCtx, 0);
        return ticket;
    }

    Ticket* getGroupSizeAsync(std::vector<uint32_t> hashes, void* outputBuffer = nullptr) {
        Ticket* ticket = FractalSDK::SDK::Get()->allocateTicket();
        auto* taskCtx = new getGroupSizeTask();
        taskCtx->hashes = std::move(hashes);
        taskCtx->context.componentHashIds = taskCtx->hashes.data();
        taskCtx->context.count = static_cast<uint32_t>(taskCtx->hashes.size());
        taskCtx->context.domainId = CMDomainId;
        taskCtx->targetBuffer = outputBuffer;
        taskCtx->ticket = ticket;

        auto fn = [](void* ctx) {
            auto data = static_cast<getGroupSizeTask*>(ctx);
            FURCMDPacket packet;
            packet.methodHash = getGroupSizeHash;
            packet.payload = &data->context;
            packet.payloadSize = sizeof(getGroupSizeInstanceCMDContext);
            packet.outputBuffer = data->targetBuffer;
            packet.fence = (uint64_t*)&data->ticket->fence;
            FractalSDK::SDK::Get()->sendPacket(packet);
            delete data;
        };
        FractalSDK::WorkScheduler::scheduleTask(fn, taskCtx);
        return ticket; 
    }

    Ticket* getRawPtrAsync(uint32_t componentId, bool lock = false, void* outputBuffer = nullptr) {
        Ticket* ticket = FractalSDK::SDK::Get()->allocateTicket();
        auto* taskCtx = new getRawPtrTask();
        taskCtx->ticket = ticket;
        taskCtx->context.componentId = componentId;
        taskCtx->context.domainId = CMDomainId;
        taskCtx->context.lock = lock;
        taskCtx->targetBuffer = outputBuffer;

        auto fn = [](void* ctx) {
            auto data = static_cast<getRawPtrTask*>(ctx);
            FURCMDPacket packet;
            packet.methodHash = getRawPtrHash;
            packet.payload = &data->context;
            packet.payloadSize = sizeof(getRawPtrInstanceCMDContext);
            packet.outputBuffer = data->targetBuffer;
            packet.fence = (uint64_t*)&data->ticket->fence;
            FractalSDK::SDK::Get()->sendPacket(packet);
            delete data;
        };
        FractalSDK::WorkScheduler::scheduleTask(fn, taskCtx);
        return ticket;
    }

    void setLock(uint32_t componentId, bool lock) {
        setComponentLockCMDContext context;
        context.domainId = CMDomainId;
        context.componentId = componentId;
        context.lock = lock;

        FURCMDPacket packet;
        packet.methodHash = setComponentLockHash;
        packet.payload = &context;
        packet.payloadSize = sizeof(context);

        FractalSDK::SDK::Get()->sendPacket(packet);
    }

    bool isLocked(uint32_t componentId) {
        Ticket* ticket = FractalSDK::SDK::Get()->allocateTicket();
        getComponentLockCMDContext context;
        context.domainId = CMDomainId;
        context.componentId = componentId;

        std::atomic_bool* lockPtr = nullptr;
        FURCMDPacket packet;
        packet.methodHash = getComponentLockHash;
        packet.payload = &context;
        packet.payloadSize = sizeof(context);
        packet.outputBuffer = &lockPtr;
        packet.fence = (uint64_t*)&ticket->fence;

        FractalSDK::SDK::Get()->sendPacket(packet);
        while(!ticket->isReady()) {}
        return lockPtr ? lockPtr->load(std::memory_order_acquire) : false;
    }

    void resizeComponent(uint32_t componentId, uint32_t newCapacity) {
        resizeComponentCMDContext context;
        context.domainId = CMDomainId;
        context.componentId = componentId;
        context.newCapacity = newCapacity;

        FURCMDPacket packet;
        packet.methodHash = resizeComponentHash;
        packet.payloadSize = sizeof(context);
        packet.payload = &context;

        FractalSDK::SDK::Get()->sendPacket(packet);
    }

    std::string getDomainName() { return CMDomainName; }
    uint32_t getDomainHashId() { return CMDomainId; }

    void destroyDomain(uint32_t vaultId) {
        destroyDomainCMDContext context;
        context.domainId = CMDomainId;
        context.vaultId = vaultId;

        FURCMDPacket packet;
        packet.methodHash = destroyDomainHash;
        packet.payloadSize = sizeof(context);
        packet.payload = &context;

        FractalSDK::SDK::Get()->sendPacket(packet);
    }

private:
    uint32_t CMDomainId;
    std::string CMDomainName;

    struct GetComponentTask { getComponentInstanceCMDContext context; Ticket* ticket; void** targetBuffer; };
    struct hasComponentTask { hasComponentInstanceCMDContext context; Ticket* ticket; void* targetBuffer; };
    struct containsTask { containsInstanceCMDContext context; Ticket* ticket; void* targetBuffer; std::vector<uint32_t> hashes; };
    struct getGroupSizeTask { getGroupSizeInstanceCMDContext context; Ticket* ticket; void* targetBuffer; std::vector<uint32_t> hashes; };
    struct getRawPtrTask { getRawPtrInstanceCMDContext context; Ticket* ticket; void* targetBuffer; };
};
