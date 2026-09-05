#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>
#include "ankerl/unordered_dense.h"
#include <SDK/archetyped/hash/hash.h>
#include "ComponentData.h"
#include "ComponentRegistry.h"

class VaultInstance {
public:
    VaultInstance();
    explicit VaultInstance(ComponentRegistry* registry, bool ownRegistry = false);
    ~VaultInstance();

    VaultInstance(const VaultInstance&) = delete;
    VaultInstance& operator=(const VaultInstance&) = delete;
    VaultInstance(VaultInstance&&) = delete;
    VaultInstance& operator=(VaultInstance&&) = delete;

    void setRegistry(ComponentRegistry* registry, bool ownRegistry = false);
    ComponentRegistry* getRegistry() const;

    void registerTable(uint32_t hashId, size_t elementSize, size_t capacity);
    void registerTable(uint32_t hashId, size_t capacity);
    void linkTable(uint32_t hashId, ComponentData* source);
    void resizeTable(uint32_t hashId, size_t newCapacity);
    ComponentData* getTable(uint32_t hashId) const;
    bool hasTable(uint32_t hashId) const;
    size_t getTableCount() const;

    ComponentRegistry* releaseRegistry();

    std::vector<uint32_t> getAllTableHashes() const;

private:
    struct TableEntry {
        ComponentData* data;
        bool owned;
    };

    ComponentRegistry* registry;
    bool ownsRegistry;
    ankerl::unordered_dense::map<uint32_t, TableEntry, IdentityHash> tables;
    std::vector<std::unique_ptr<ComponentData>> ownedTables;
    std::vector<std::unique_ptr<EngineArena>> arenas;
};
