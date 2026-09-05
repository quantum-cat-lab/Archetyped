#include "ComponentInstance.h"

#include <atomic>
#include <stdexcept>
#include <algorithm>
#include <cstring>
#include <iostream>

#include "../../MemoryAlloc/EngineArena.h"

ComponentInstance::ComponentInstance()
    : ownsVault(true) {
    vault = new VaultInstance();
    cmdMem = std::make_unique<EngineArena>(2 * 1024 * 1024);
}

ComponentInstance::ComponentInstance(VaultInstance* v)
    : vault(v), ownsVault(false) {
    cmdMem = std::make_unique<EngineArena>(2 * 1024 * 1024);
}

ComponentInstance::~ComponentInstance() {
    if (ownsVault) {
        delete vault;
    }
}

void ComponentInstance::attachComponent(const AttachCMD& cmd) {
    attachComponent(cmd.entity, cmd.compHashId, cmd.data);
}

void ComponentInstance::removeComponent(const RemoveCMD& cmd) {
    removeComponent(cmd.entity, cmd.compHashId);
}

void ComponentInstance::flushCommands() {
    state.store(ECSState::Syncing);

    for (const auto& cmd : attachQueue) {
        attachComponent(cmd.entity, cmd.compHashId, cmd.data);
    }
    for (const auto& cmd : removeQueue) {
        removeComponent(cmd.entity, cmd.compHashId);
    }

    attachQueue.clear();
    removeQueue.clear();

    if (cmdMem) {
        cmdMem->reset();
    }

    state.store(ECSState::Idle);
}

void ComponentInstance::registerComponent(const uint32_t hashId, size_t elementSize, size_t capacity) {
    if (vault->hasTable(hashId)) return;
    vault->registerTable(hashId, elementSize, capacity);
}

void ComponentInstance::resizeComponent(const uint32_t hashId, size_t newCapacity) {
    vault->resizeTable(hashId, newCapacity);
}

void ComponentInstance::attachComponentDeferred(Entity e, const uint32_t hashId, void* data, size_t size) {
    if (state.load() == ECSState::Processing) {
        std::lock_guard<std::mutex> lock(cmdLock);
        size_t actualSize = getComponentSize(hashId);
        void* arenaMem = cmdMem->allocate(actualSize, 16);
        std::memset(arenaMem, 0, actualSize);
        std::memcpy(arenaMem, data, size);
        attachQueue.push_back({hashId, e, arenaMem, actualSize});
    } else {
        attachComponent(e, hashId, data);
    }
}

void ComponentInstance::removeComponentDeferred(Entity e, const uint32_t hashId) {
    if (state.load() == ECSState::Processing) {
        std::lock_guard<std::mutex> lock(cmdLock);
        removeQueue.push_back({hashId, e});
    } else {
        removeComponent(e, hashId);
    }
}

void* ComponentInstance::getComponent(Entity e, const uint32_t hashId) {
    if (state.load() == ECSState::Syncing) {
        throw std::runtime_error("Race condition: tried to read component while syncing");
    }
    ComponentData* cd = vault->getTable(hashId);
    if (!cd) return nullptr;
    if (!cd->has(e)) return nullptr;
    return cd->get(e);
}

bool ComponentInstance::hasComponent(Entity e, const uint32_t hashId) {
    ComponentData* cd = vault->getTable(hashId);
    if (!cd) return false;
    return cd->has(e);
}

ComponentData* ComponentInstance::getComponentData(const uint32_t hashId) {
    return vault->getTable(hashId);
}

void ComponentInstance::onComponentAttached(Entity e, uint32_t hashId) {
    for (auto& group : groups) {
        if (contains(group.components, group.count, hashId)) {
            if (hasAll(e, group.components, group.count)) {
                for (size_t i = 0; i < group.count; ++i) {
                    uint32_t compId = group.components[i];
                    ComponentData* cd = vault->getTable(compId);
                    if (!cd) continue;

                    if (cd->elementSize > 256) {
                        if (scratchBuffer.size() < cd->elementSize) {
                            scratchBuffer.resize(cd->elementSize);
                        }
                    }
                    cd->moveToGroup(e, scratchBuffer.data());
                }
                group.size++;
            }
        }
    }
}

void ComponentInstance::onComponentRemoved(Entity e, const uint32_t hashId) {
    for (auto& group : groups) {
        if (contains(group.components, group.count, hashId)) {
            bool wasInGroup = true;
            for (size_t i = 0; i < group.count; ++i) {
                uint32_t compId = group.components[i];
                ComponentData* cd = vault->getTable(compId);
                if (!cd) {
                    wasInGroup = false;
                    break;
                }

                uint32_t sparse = cd->getSparse(e.id);
                if (sparse >= cd->groupedCount) {
                    wasInGroup = false;
                    break;
                }
            }

            if (wasInGroup) {
                for (size_t i = 0; i < group.count; ++i) {
                    uint32_t compId = group.components[i];
                    ComponentData* cd = vault->getTable(compId);
                    if (!cd) continue;

                    if (cd->elementSize > 256) {
                        if (scratchBuffer.size() < cd->elementSize) {
                            scratchBuffer.resize(cd->elementSize);
                        }
                    }

                    uint32_t currentIdx = cd->getSparse(e.id);
                    cd->groupedCount--;

                    cd->swapEntities(currentIdx, (uint32_t)cd->groupedCount, scratchBuffer.data());
                }
                group.size--;
            }
        }
    }
}

void ComponentInstance::registerGroup(const uint32_t* componentHashIds, size_t count) {
    Group newGroup;
    newGroup.count = count;
    newGroup.size = 0;
    newGroup.components = (uint32_t*)malloc(count * sizeof(uint32_t));
    std::memcpy(newGroup.components, componentHashIds, count * sizeof(uint32_t));

    groups.push_back(newGroup);
}

void ComponentInstance::attachComponent(Entity e, const uint32_t hashId, void* data) {
    ComponentData* cd = vault->getTable(hashId);
    if (!cd) throw std::runtime_error("Component not registered");
    cd->attach(e, data);
    onComponentAttached(e, hashId);
}

void ComponentInstance::removeComponent(Entity e, const uint32_t hashId) {
    onComponentRemoved(e, hashId);
    ComponentData* cd = vault->getTable(hashId);
    if (!cd) return;
    cd->remove(e);
}

bool ComponentInstance::contains(const uint32_t* array, size_t count, uint32_t value) {
    for (size_t i = 0; i < count; ++i) {
        if (array[i] == value) return true;
    }
    return false;
}

bool ComponentInstance::hasAll(Entity e, const uint32_t* comps, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        if (!hasComponent(e, comps[i])) return false;
    }
    return true;
}

size_t ComponentInstance::getComponentSize(const uint32_t hashId) {
    ComponentData* cd = vault->getTable(hashId);
    if (!cd) return 0;
    return cd->elementSize;
}

size_t ComponentInstance::getGroupSize(const uint32_t* compNames, size_t count) {
    for (const auto& group : groups) {
        if (group.count == count &&
            std::memcmp(group.components, compNames, count * sizeof(uint32_t)) == 0) {
            return group.size;
        }
    }
    return 0;
}

std::atomic_bool& ComponentInstance::getComponentLock(const uint32_t hashId) {
    auto it = componentLocks.find(hashId);
    if (it == componentLocks.end()) {
        componentLocks[hashId] = false;
    }
    return componentLocks[hashId];
}

void ComponentInstance::setComponentLock(const uint32_t hashId, bool lock) {
    getComponentLock(hashId).store(lock, std::memory_order_release);
}

void* ComponentInstance::getRawPtr(const uint32_t hashId, bool lock) {
    ComponentData* cd = vault->getTable(hashId);
    if (!cd) return nullptr;
    if (lock) {
        getComponentLock(hashId).store(true, std::memory_order_release);
    }
    return cd->getRawPtr();
}

uint32_t* ComponentInstance::getDensePtr(const uint32_t hashId) {
    ComponentData* cd = vault->getTable(hashId);
    if (!cd) return nullptr;
    return cd->dense.data();
}

uint32_t* ComponentInstance::getDenseIndexPtr(const uint32_t hashId) {
    ComponentData* cd = vault->getTable(hashId);
    if (!cd) return nullptr;
    return cd->getDenseIndexPtr();
}

void ComponentInstance::destroyEntity(Entity e) {
    std::vector<uint32_t> allHashes = vault->getAllTableHashes();
    std::vector<uint32_t> toRemove;
    for (uint32_t hashId : allHashes) {
        ComponentData* cd = vault->getTable(hashId);
        if (cd && cd->has(e)) {
            toRemove.push_back(hashId);
        }
    }
    for (uint32_t hashId : toRemove) {
        removeComponent(e, hashId);
    }
}

void ComponentInstance::queryEntities(uint32_t* componentHashIds, uint32_t count, Entity* outputBuffer) {
    for (const auto& group : groups) {
        if (group.count == count &&
            std::memcmp(group.components, componentHashIds, count * sizeof(uint32_t)) == 0) {

            if (group.size == 0) return;

            uint32_t firstCompId = group.components[0];
            ComponentData* cd = vault->getTable(firstCompId);
            if (!cd) return;

            for (size_t i = 0; i < group.size; ++i) {
                outputBuffer[i] = Entity{ cd->dense[i], 0 };
            }
            return;
        }
    }
}

VaultInstance* ComponentInstance::getVault() const {
    return vault;
}

void ComponentInstance::setVault(VaultInstance* v, bool own) {
    if (ownsVault && vault) {
        delete vault;
    }
    vault = v;
    ownsVault = own;
}
