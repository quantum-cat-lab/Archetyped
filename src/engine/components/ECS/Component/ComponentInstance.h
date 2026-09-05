#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <cstring>
#include <string>

#include "ComponentData.h"
#include "VaultInstance.h"
#include <SDK/archetyped/hash/hash.h>
#include "ankerl/unordered_dense.h"
enum class ECSState {
    Idle,
    Processing,
    Syncing
};
struct AttachCMD {
    uint32_t compHashId;
    Entity entity;
    void* data;
    size_t size;
};
struct RemoveCMD {
    uint32_t compHashId;
    Entity entity;
};

struct Group {
    uint32_t* components;
    size_t count;
    size_t size;
};
class EngineArena; // forward

class ComponentInstance {
public:
    ComponentInstance();
    explicit ComponentInstance(VaultInstance* v);
    ~ComponentInstance();

    ComponentInstance(const ComponentInstance&) = delete;
    ComponentInstance& operator=(const ComponentInstance&) = delete;
    ComponentInstance(ComponentInstance&&) = delete;
    ComponentInstance& operator=(ComponentInstance&&) = delete;

    void attachComponent(const AttachCMD& cmd);
    void removeComponent(const RemoveCMD& cmd);
    void flushCommands();

    void registerComponent(const uint32_t hashId, size_t elementSize, size_t capacity);
    void resizeComponent(const uint32_t hashId, size_t newCapacity);

    void attachComponentDeferred(Entity e, const uint32_t hashId, void* data, size_t size);
    void removeComponentDeferred(Entity e, const uint32_t hashId);

    void* getComponent(Entity e, const uint32_t hashId);
    bool hasComponent(Entity e, const uint32_t hashId);
    ComponentData* getComponentData(const uint32_t hashId);

    void onComponentAttached(Entity e, uint32_t hashId);
    void onComponentRemoved(Entity e, const uint32_t hashId);

    void registerGroup(const uint32_t* componentHashIds, size_t count);

    bool contains(const uint32_t* array, size_t count, uint32_t value);
    bool hasAll(Entity e, const uint32_t* comps, size_t count);
    size_t getGroupSize(const uint32_t* compNames, size_t count);
    void queryEntities(uint32_t* componentHashIds, uint32_t count, Entity* outputBuffer);

    void* getRawPtr(const uint32_t hashId, bool lock = false);
    uint32_t* getDensePtr(const uint32_t hashId);
    uint32_t* getDenseIndexPtr(const uint32_t hashId);
    size_t getComponentSize(const uint32_t hashId);

    std::atomic_bool& getComponentLock(const uint32_t hashId);
    void setComponentLock(const uint32_t hashId, bool lock);
    void destroyEntity(Entity e);

    VaultInstance* getVault() const;
    void setVault(VaultInstance* v, bool own = false);

private:
    void attachComponent(Entity e, const uint32_t hashId, void* data);
    void removeComponent(Entity e, const uint32_t hashId);

    VaultInstance* vault;
    bool ownsVault;

    std::unordered_map<uint32_t, std::atomic_bool> componentLocks;
    std::vector<Group> groups;

    std::atomic<ECSState> state{ECSState::Idle};
    std::vector<AttachCMD> attachQueue;
    std::vector<RemoveCMD> removeQueue;
    std::mutex cmdLock;
    std::unique_ptr<EngineArena> cmdMem = nullptr;
    std::vector<char> scratchBuffer;
};
