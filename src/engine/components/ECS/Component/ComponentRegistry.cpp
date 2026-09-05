#include "ComponentRegistry.h"

void ComponentRegistry::registerType(uint32_t hashId, size_t elementSize, size_t alignment) {
    types[hashId] = {elementSize, alignment};
}

const ComponentTypeInfo* ComponentRegistry::getTypeInfo(uint32_t hashId) const {
    auto it = types.find(hashId);
    if (it == types.end()) return nullptr;
    return &it->second;
}

bool ComponentRegistry::hasType(uint32_t hashId) const {
    return types.find(hashId) != types.end();
}

size_t ComponentRegistry::getElementSize(uint32_t hashId) const {
    auto it = types.find(hashId);
    if (it == types.end()) return 0;
    return it->second.elementSize;
}

size_t ComponentRegistry::getAlignment(uint32_t hashId) const {
    auto it = types.find(hashId);
    if (it == types.end()) return 16;
    return it->second.alignment;
}
