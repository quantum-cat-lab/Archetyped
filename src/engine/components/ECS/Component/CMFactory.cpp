#include "CMFactory.h"
#include "../../MemoryAlloc/EngineArena.h"
#include "ComponentInstance.h"
#include "ComponentRegistry.h"
#include "VaultInstance.h"
#include <SDK/archetyped/hash/hash.h>
#include "FURCMD/FURCMD.h"
#include "core/FractalKernel.h"
#include <atomic>
#include <cstdint>
#include <cstring>

constexpr uint32_t registerCMDomainHash = fnv1aHashConst("archetyped:ecs:registerCMInstance");
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

// --- New Registry/Vault hashes ---
constexpr uint32_t createRegistryHash = fnv1aHashConst("archetyped:ecs:createRegistry");
constexpr uint32_t registerTypeHash = fnv1aHashConst("archetyped:ecs:registerType");
constexpr uint32_t createVaultHash = fnv1aHashConst("archetyped:ecs:createVault");
constexpr uint32_t bindRegistryToVaultHash = fnv1aHashConst("archetyped:ecs:bindRegistryToVault");
constexpr uint32_t registerDomainWithVaultHash = fnv1aHashConst("archetyped:ecs:registerInstanceWithVault");
constexpr uint32_t registerTableFromRegistryHash = fnv1aHashConst("archetyped:ecs:registerTableFromRegistry");
constexpr uint32_t linkComponentTableHash = fnv1aHashConst("archetyped:ecs:linkComponentTable");
constexpr uint32_t destroyDomainHash = fnv1aHashConst("archetyped:ecs:destroyInstance");

ankerl::unordered_dense::map<uint32_t, ComponentInstance*, IdentityHash> CMFactory::componentManagers;
ankerl::unordered_dense::map<uint32_t, ComponentRegistry*, IdentityHash> CMFactory::registries;
ankerl::unordered_dense::map<uint32_t, VaultInstance*, IdentityHash> CMFactory::vaults;

CMFactory::CMFactory(){
    FractalKernel::instance().registerCMDMethod(registerCMDomainHash, &registerCMDomain);
    FractalKernel::instance().registerCMDMethod(flushCommandsHash, &flushCommandsCMD);
    FractalKernel::instance().registerCMDMethod(registerComponentHash, &registerComponentCMD);
    FractalKernel::instance().registerCMDMethod(resizeComponentHash, &resizeComponentCMD);
    FractalKernel::instance().registerCMDMethod(attachComponentDeferredHash, &attachComponentDeferredCMD);
    FractalKernel::instance().registerCMDMethod(removeComponentDeferredHash, &removeComponentDeferredCMD);
    FractalKernel::instance().registerCMDMethod(getComponentHash, &getComponentCMD);
    FractalKernel::instance().registerCMDMethod(hasComponentHash, &hasComponentCMD);
    FractalKernel::instance().registerCMDMethod(onComponentAttachedHash, &onComponentAttachedCMD);
    FractalKernel::instance().registerCMDMethod(onComponentRemovedHash, &onComponentRemovedCMD);
    FractalKernel::instance().registerCMDMethod(registerGroupHash, &registerGroupCMD);
    FractalKernel::instance().registerCMDMethod(containsHash, &containsCMD);
    FractalKernel::instance().registerCMDMethod(hasAllHash, &hasAllCMD);
    FractalKernel::instance().registerCMDMethod(getGroupSizeHash, &getGroupSizeCMD);
    FractalKernel::instance().registerCMDMethod(queryEntitiesHash, &queryEntitiesCMD);
    FractalKernel::instance().registerCMDMethod(getRawPtrHash, &getRawPtrCMD);
    FractalKernel::instance().registerCMDMethod(getDensePtrHash, &getDensePtrCMD);
    FractalKernel::instance().registerCMDMethod(getComponentLockHash, &getComponentLockCMD);
    FractalKernel::instance().registerCMDMethod(setComponentLockHash, &setComponentLockCMD);
    FractalKernel::instance().registerCMDMethod(destroyEntityHash, &destroyEntityCMD);
    FractalKernel::instance().registerCMDMethod(getDenseIndexPtrHash, &getDenseIndexPtrCMD);

    // --- New Registry/Vault handlers ---
    FractalKernel::instance().registerCMDMethod(createRegistryHash, &createRegistryCMD);
    FractalKernel::instance().registerCMDMethod(registerTypeHash, &registerTypeCMD);
    FractalKernel::instance().registerCMDMethod(createVaultHash, &createVaultCMD);
    FractalKernel::instance().registerCMDMethod(bindRegistryToVaultHash, &bindRegistryToVaultCMD);
    FractalKernel::instance().registerCMDMethod(registerDomainWithVaultHash, &registerDomainWithVaultCMD);
    FractalKernel::instance().registerCMDMethod(registerTableFromRegistryHash, &registerTableFromRegistryCMD);
    FractalKernel::instance().registerCMDMethod(linkComponentTableHash, &linkComponentTableCMD);
    FractalKernel::instance().registerCMDMethod(destroyDomainHash, &destroyDomainCMD);
}

void CMFactory::resizeComponentCMD(FURCMDPacket& packet) {
    resizeComponentCMDContext& context = *reinterpret_cast<resizeComponentCMDContext*>(packet.payload);
    uint32_t domainId = context.domainId;
    if (componentManagers.find(domainId) != componentManagers.end()){
        componentManagers[domainId]->resizeComponent(context.componentId, context.newCapacity);
    }
}
void CMFactory::registerCMDomain(FURCMDPacket& packet) {
    uint32_t domainId = *reinterpret_cast<uint32_t*>(packet.payload);
    if (componentManagers.find(domainId) == componentManagers.end()){
        componentManagers[domainId] = new ComponentInstance();
    }
}
void CMFactory::attachComponentDeferredCMD(FURCMDPacket &packet) {
    attachComponentDeferredInstanceCMDContext& context = *reinterpret_cast<attachComponentDeferredInstanceCMDContext*>(packet.payload);
    uint32_t domainId = context.domainId;
    if (componentManagers.find(domainId) != componentManagers.end()){
        componentManagers[domainId]->attachComponentDeferred(context.entity, context.componentId, context.componentData, context.dataSize);
    }
}
void CMFactory::removeComponentDeferredCMD(FURCMDPacket &packet){
    removeComponentDeferredInstanceCMDContext& context = *reinterpret_cast<removeComponentDeferredInstanceCMDContext*>(packet.payload);
    uint32_t domainId = context.domainId;
    if (componentManagers.find(domainId) != componentManagers.end()){
        componentManagers[domainId]->removeComponentDeferred(context.entity, context.componentId);
    }
}
void CMFactory::registerComponentCMD(FURCMDPacket &packet){
    registerComponentInstanceCMDContext& context = *reinterpret_cast<registerComponentInstanceCMDContext*>(packet.payload);
    uint32_t domainId = context.domainId;
    if (componentManagers.find(domainId) != componentManagers.end()){
        componentManagers[domainId]->registerComponent(context.componentId, context.componentSize, context.capacity);
    }
}
void CMFactory::flushCommandsCMD(FURCMDPacket &packet){
    flushCommandsInstanceCMDContext& context = *reinterpret_cast<flushCommandsInstanceCMDContext*>(packet.payload);
    uint32_t domainId = context.domainId;
    if (componentManagers.find(domainId) != componentManagers.end()){
        componentManagers[domainId]->flushCommands();
    }
}
void CMFactory::getComponentCMD(FURCMDPacket &packet) {
    getComponentInstanceCMDContext& context = *reinterpret_cast<getComponentInstanceCMDContext*>(packet.payload);
    uint32_t domainId = context.domainId;
    auto it = componentManagers.find(domainId);  
    if (it != componentManagers.end()) {
        auto* manager = it->second;
        void* compPtr = manager->getComponent(context.entity, context.componentId);
        if (packet.outputBuffer) {
            *reinterpret_cast<void**>(packet.outputBuffer) = compPtr;
        }
    } else {

    }
}
void CMFactory::hasComponentCMD(FURCMDPacket &packet) {
    hasComponentInstanceCMDContext& context = *reinterpret_cast<hasComponentInstanceCMDContext*>(packet.payload);
    uint32_t domainId = context.domainId;
    auto it = componentManagers.find(domainId);
    if (it != componentManagers.end()) {
        auto* manager = it->second;
        bool hasComp = manager->hasComponent(context.entity, context.componentId);
        *reinterpret_cast<bool*>(packet.outputBuffer) = hasComp;
    }
}
void CMFactory::registerGroupCMD(FURCMDPacket &packet) {
    registerGroupInstanceCMDContext& context = *reinterpret_cast<registerGroupInstanceCMDContext*>(packet.payload);
    uint32_t domainId = context.domainId;
    if (componentManagers.find(domainId) != componentManagers.end()){
        componentManagers[domainId]->registerGroup(context.componentHashIds, context.count);
    }
}
void CMFactory::onComponentAttachedCMD(FURCMDPacket &packet) {
    onComponentAttachedInstanceCMDContext& context = *reinterpret_cast<onComponentAttachedInstanceCMDContext*>(packet.payload);
    uint32_t domainId = context.domainId;
    if (componentManagers.find(domainId) != componentManagers.end()){
        componentManagers[domainId]->onComponentAttached(context.entity, context.componentId);
    }
}
void CMFactory::onComponentRemovedCMD(FURCMDPacket &packet) {
    onComponentRemovedInstanceCMDContext& context = *reinterpret_cast<onComponentRemovedInstanceCMDContext*>(packet.payload);
    uint32_t domainId = context.domainId;
    if (componentManagers.find(domainId) != componentManagers.end()){
        componentManagers[domainId]->onComponentRemoved(context.entity, context.componentId);
    }
}
void CMFactory::containsCMD(FURCMDPacket &packet) {
    containsInstanceCMDContext& context = *reinterpret_cast<containsInstanceCMDContext*>(packet.payload);
    uint32_t domainId = context.domainId;
    auto it = componentManagers.find(domainId);
    if (it != componentManagers.end()) {
        auto* manager = it->second;
        bool result = manager->contains(context.componentHashIds, context.count, context.value);
        *reinterpret_cast<bool*>(packet.outputBuffer) = result;
    }
}
void CMFactory::hasAllCMD(FURCMDPacket &packet) {
    hasAllInstanceCMDContext& context = *reinterpret_cast<hasAllInstanceCMDContext*>(packet.payload);
    uint32_t domainId = context.domainId;
    auto it = componentManagers.find(domainId);
    if (it != componentManagers.end()) {
        auto* manager = it->second;
        bool result = manager->hasAll(context.entity, context.componentHashIds, context.count);
        *reinterpret_cast<bool*>(packet.outputBuffer) = result;
    }
}
void CMFactory::getGroupSizeCMD(FURCMDPacket &packet) {
    getGroupSizeInstanceCMDContext& context = *reinterpret_cast<getGroupSizeInstanceCMDContext*>(packet.payload);
    uint32_t domainId = context.domainId;
    auto it = componentManagers.find(domainId);
    if (it != componentManagers.end()) {
        auto* manager = it->second;
        uint32_t groupSize = (uint32_t)manager->getGroupSize(context.componentHashIds, context.count);
        *reinterpret_cast<uint32_t*>(packet.outputBuffer) = groupSize;
    }
}

void CMFactory::queryEntitiesCMD(FURCMDPacket &packet) {
    queryEntitiesInstanceCMDContext& context = *reinterpret_cast<queryEntitiesInstanceCMDContext*>(packet.payload);
    uint32_t domainId = context.domainId;
    auto it = componentManagers.find(domainId);
    if (it != componentManagers.end()) {
        auto* manager = it->second;
        manager->queryEntities(context.componentHashIds, context.count, static_cast<Entity*>(packet.outputBuffer));
    }
}

void CMFactory::getRawPtrCMD(FURCMDPacket &packet) {
    getRawPtrInstanceCMDContext& context = *reinterpret_cast<getRawPtrInstanceCMDContext*>(packet.payload);
    uint32_t domainId = context.domainId;
    auto it = componentManagers.find(domainId);
    if (it != componentManagers.end()) {
        auto* manager = it->second;
        void* rawPtr = manager->getRawPtr(context.componentId, context.lock);
        *reinterpret_cast<void**>(packet.outputBuffer) = rawPtr;
    }
}

void CMFactory::getDensePtrCMD(FURCMDPacket &packet) {
    getDensePtrInstanceCMDContext& context = *reinterpret_cast<getDensePtrInstanceCMDContext*>(packet.payload);
    uint32_t domainId = context.domainId;
    auto it = componentManagers.find(domainId);
    if (it != componentManagers.end()) {
        auto* manager = it->second;
        uint32_t* densePtr = manager->getDensePtr(context.componentId);
        *reinterpret_cast<uint32_t**>(packet.outputBuffer) = densePtr;
    }
}

void CMFactory::getComponentLockCMD(FURCMDPacket &packet) {

    getComponentLockCMDContext& context = *reinterpret_cast<getComponentLockCMDContext*>(packet.payload);
    uint32_t domainId = context.domainId;
    auto it = componentManagers.find(domainId);
    if (it != componentManagers.end()) {
        auto* manager = it->second;
        std::atomic_bool& lock = manager->getComponentLock(context.componentId);
        *reinterpret_cast<std::atomic_bool**>(packet.outputBuffer) = &lock;
    }
}
void CMFactory::setComponentLockCMD(FURCMDPacket &packet) {
    setComponentLockCMDContext& context = *reinterpret_cast<setComponentLockCMDContext*>(packet.payload);
    uint32_t domainId = context.domainId;
    auto it = componentManagers.find(domainId);
    if (it != componentManagers.end()) {
        auto* manager = it->second;
        manager->setComponentLock(context.componentId, context.lock);
    }
}

void CMFactory::destroyEntityCMD(FURCMDPacket& packet) {
    destroyEntityCMDContext& context = *reinterpret_cast<destroyEntityCMDContext*>(packet.payload);
    uint32_t domainId = context.domainId;
    Entity e = context.entity;
    auto it = componentManagers.find(domainId);
    if (it != componentManagers.end()) {
        it->second->destroyEntity(e);
    }
}

void CMFactory::getDenseIndexPtrCMD(FURCMDPacket& packet) {
    getDenseIndexPtrCMDContext& context = *reinterpret_cast<getDenseIndexPtrCMDContext*>(packet.payload);
    uint32_t domainId = context.domainId;
    auto it = componentManagers.find(domainId);
    if (it != componentManagers.end()) {
        auto* manager = it->second;
        uint32_t* ptr = manager->getDenseIndexPtr(context.componentId);
        *reinterpret_cast<uint32_t**>(packet.outputBuffer) = ptr;
    }
}

// --- New Registry/Vault handlers ---

void CMFactory::createRegistryCMD(FURCMDPacket& packet) {
    createRegistryCMDContext& context = *reinterpret_cast<createRegistryCMDContext*>(packet.payload);
    uint32_t registryId = context.registryId;
    if (registries.find(registryId) == registries.end()) {
        registries[registryId] = new ComponentRegistry();
    }
}

void CMFactory::registerTypeCMD(FURCMDPacket& packet) {
    registerTypeCMDContext& context = *reinterpret_cast<registerTypeCMDContext*>(packet.payload);
    uint32_t registryId = context.registryId;
    auto it = registries.find(registryId);
    if (it != registries.end()) {
        it->second->registerType(context.componentId, context.componentSize, context.alignment);
    }
}

void CMFactory::createVaultCMD(FURCMDPacket& packet) {
    createVaultCMDContext& context = *reinterpret_cast<createVaultCMDContext*>(packet.payload);
    uint32_t vaultId = context.vaultId;
    if (vaults.find(vaultId) != vaults.end()) return;

    if (context.registryId != 0xFFFFFFFF) {
        auto regIt = registries.find(context.registryId);
        if (regIt != registries.end()) {
            vaults[vaultId] = new VaultInstance(regIt->second, false);
        } else {
            vaults[vaultId] = new VaultInstance();
        }
    } else {
        vaults[vaultId] = new VaultInstance();
    }
}

void CMFactory::bindRegistryToVaultCMD(FURCMDPacket& packet) {
    bindRegistryToVaultCMDContext& context = *reinterpret_cast<bindRegistryToVaultCMDContext*>(packet.payload);
    auto vaultIt = vaults.find(context.vaultId);
    auto regIt = registries.find(context.registryId);
    if (vaultIt != vaults.end() && regIt != registries.end()) {
        vaultIt->second->setRegistry(regIt->second, false);
    }
}

void CMFactory::registerDomainWithVaultCMD(FURCMDPacket& packet) {
    registerDomainWithVaultCMDContext& context = *reinterpret_cast<registerDomainWithVaultCMDContext*>(packet.payload);
    uint32_t domainId = context.domainId;
    if (componentManagers.find(domainId) != componentManagers.end()) {
        auto vaultIt = vaults.find(context.vaultId);
        if (vaultIt != vaults.end()) {
            componentManagers[domainId]->setVault(vaultIt->second);
        }
        return;
    }

    auto vaultIt = vaults.find(context.vaultId);
    if (vaultIt != vaults.end()) {
        componentManagers[domainId] = new ComponentInstance(vaultIt->second);
    }
}

void CMFactory::linkComponentTableCMD(FURCMDPacket& packet) {
    linkComponentTableCMDContext& context = *reinterpret_cast<linkComponentTableCMDContext*>(packet.payload);
    auto srcIt = componentManagers.find(context.sourceDomainId);
    auto dstIt = componentManagers.find(context.targetDomainId);
    if (srcIt == componentManagers.end() || dstIt == componentManagers.end()) return;

    ComponentData* cd = srcIt->second->getComponentData(context.componentId);
    if (!cd) return;

    dstIt->second->getVault()->linkTable(context.componentId, cd);
}

void CMFactory::registerTableFromRegistryCMD(FURCMDPacket& packet) {
    registerTableFromRegistryCMDContext& context = *reinterpret_cast<registerTableFromRegistryCMDContext*>(packet.payload);
    uint32_t domainId = context.domainId;
    auto it = componentManagers.find(domainId);
    if (it == componentManagers.end()) return;

    VaultInstance* v = it->second->getVault();
    if (!v) return;
    if (!v->getRegistry()) return;
    if (v->hasTable(context.componentId)) return;

    v->registerTable(context.componentId, context.capacity);
}

void CMFactory::destroyDomainCMD(FURCMDPacket& packet) {
    destroyDomainCMDContext& context = *reinterpret_cast<destroyDomainCMDContext*>(packet.payload);
    uint32_t domainId = context.domainId;
    uint32_t vaultId = context.vaultId;

    auto cmIt = componentManagers.find(domainId);
    if (cmIt != componentManagers.end()) {
        delete cmIt->second;
        componentManagers.erase(cmIt);
    }

    auto vIt = vaults.find(vaultId);
    if (vIt != vaults.end()) {
        delete vIt->second;
        vaults.erase(vIt);
    }
}
