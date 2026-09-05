#include "VaultInstance.h"
#include "../../MemoryAlloc/EngineArena.h"

VaultInstance::VaultInstance()
    : registry(nullptr), ownsRegistry(false) {}

VaultInstance::VaultInstance(ComponentRegistry* reg, bool ownReg)
    : registry(reg), ownsRegistry(ownReg) {}

VaultInstance::~VaultInstance() {
    if (ownsRegistry) {
        delete registry;
    }
}

void VaultInstance::setRegistry(ComponentRegistry* reg, bool ownReg) {
    if (ownsRegistry && registry) {
        delete registry;
    }
    registry = reg;
    ownsRegistry = ownReg;
}

ComponentRegistry* VaultInstance::getRegistry() const {
    return registry;
}

void VaultInstance::registerTable(uint32_t hashId, size_t elementSize, size_t capacity) {
    if (tables.find(hashId) != tables.end()) return;

    size_t alignedElementSize = (elementSize + 15) & ~15;
    size_t totalBlockSize = alignedElementSize * capacity + (2 * capacity * sizeof(uint32_t)) + 1024;

    EngineArena* arena = new EngineArena(totalBlockSize);
    arenas.push_back(std::unique_ptr<EngineArena>(arena));
    auto cd = std::make_unique<ComponentData>(elementSize, capacity, arena);
    tables[hashId] = { cd.get(), true };
    ownedTables.push_back(std::move(cd));
}

void VaultInstance::registerTable(uint32_t hashId, size_t capacity) {
    if (tables.find(hashId) != tables.end()) return;
    if (!registry) return;

    size_t elementSize = registry->getElementSize(hashId);
    if (elementSize == 0) return;

    registerTable(hashId, elementSize, capacity);
}

void VaultInstance::linkTable(uint32_t hashId, ComponentData* source) {
    if (tables.find(hashId) != tables.end()) return;
    if (!source) return;

    tables[hashId] = { source, false };
}

void VaultInstance::resizeTable(uint32_t hashId, size_t newCapacity) {
    auto it = tables.find(hashId);
    if (it == tables.end()) return;
    it->second.data->resize(newCapacity);
}

ComponentData* VaultInstance::getTable(uint32_t hashId) const {
    auto it = tables.find(hashId);
    if (it == tables.end()) return nullptr;
    return it->second.data;
}

bool VaultInstance::hasTable(uint32_t hashId) const {
    return tables.find(hashId) != tables.end();
}

size_t VaultInstance::getTableCount() const {
    return tables.size();
}

ComponentRegistry* VaultInstance::releaseRegistry() {
    ComponentRegistry* reg = registry;
    registry = nullptr;
    ownsRegistry = false;
    return reg;
}

std::vector<uint32_t> VaultInstance::getAllTableHashes() const {
    std::vector<uint32_t> hashes;
    hashes.reserve(tables.size());
    for (const auto& [hashId, _] : tables) {
        hashes.push_back(hashId);
    }
    return hashes;
}
