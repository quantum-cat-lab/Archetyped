#pragma once
#include "ComponentInstance.h"
#include "ComponentRegistry.h"
#include "VaultInstance.h"
#include "../Entity/entity.h"
#include <SDK/archetyped/hash/hash.h>
#include "engine/FURCMD/FURCMD.h"
#include <cstdint>

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

struct getGroupSizeInstanceCMDContext {
    uint32_t domainId;
    uint32_t* componentHashIds;
    uint32_t count;
};

struct queryEntitiesInstanceCMDContext {
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
struct resizeComponentCMDContext {
    uint32_t domainId;
    uint32_t componentId;
    uint32_t newCapacity;
};

struct destroyEntityCMDContext {
    uint32_t domainId;
    Entity entity;
};

struct getDenseIndexPtrCMDContext {
    uint32_t domainId;
    uint32_t componentId;
};


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

struct linkComponentTableCMDContext {
    uint32_t targetDomainId;
    uint32_t componentId;
    uint32_t sourceDomainId;
};

struct destroyDomainCMDContext {
    uint32_t domainId;
    uint32_t vaultId;
};

class CMFactory {
public:
    CMFactory();
    ~CMFactory() = default;
    static void registerCMDomain(FURCMDPacket& packet);

    static void flushCommandsCMD(FURCMDPacket& packet);
    static void registerComponentCMD(FURCMDPacket& packet);
    static void resizeComponentCMD(FURCMDPacket& packet);

    static void attachComponentDeferredCMD(FURCMDPacket& packet);
    static void removeComponentDeferredCMD(FURCMDPacket& packet);

    static void getComponentCMD(FURCMDPacket& packet);
    static void hasComponentCMD(FURCMDPacket& packet);

    static void onComponentAttachedCMD(FURCMDPacket& packet);
    static void onComponentRemovedCMD(FURCMDPacket& packet);

    static void registerGroupCMD(FURCMDPacket& packet);

    static void containsCMD(FURCMDPacket& packet);
    static void hasAllCMD(FURCMDPacket& packet);
    static void getGroupSizeCMD(FURCMDPacket& packet);
    static void queryEntitiesCMD(FURCMDPacket& packet);
    static void getRawPtrCMD(FURCMDPacket& packet);
    static void getDensePtrCMD(FURCMDPacket& packet);

    static void getComponentLockCMD(FURCMDPacket& packet);
    static void setComponentLockCMD(FURCMDPacket& packet);
    static void destroyEntityCMD(FURCMDPacket& packet);
    static void getDenseIndexPtrCMD(FURCMDPacket& packet);

    static void createRegistryCMD(FURCMDPacket& packet);
    static void registerTypeCMD(FURCMDPacket& packet);
    static void createVaultCMD(FURCMDPacket& packet);
    static void bindRegistryToVaultCMD(FURCMDPacket& packet);
    static void registerDomainWithVaultCMD(FURCMDPacket& packet);
    static void registerTableFromRegistryCMD(FURCMDPacket& packet);
    static void linkComponentTableCMD(FURCMDPacket& packet);
    static void destroyDomainCMD(FURCMDPacket& packet);

private:
    static ankerl::unordered_dense::map<uint32_t, ComponentInstance*, IdentityHash> componentManagers;
    static ankerl::unordered_dense::map<uint32_t, ComponentRegistry*, IdentityHash> registries;
    static ankerl::unordered_dense::map<uint32_t, VaultInstance*, IdentityHash> vaults;
};
