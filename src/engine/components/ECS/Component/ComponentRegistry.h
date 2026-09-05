#pragma once
#include <cstddef>
#include <cstdint>
#include "ankerl/unordered_dense.h"
#include <SDK/archetyped/hash/hash.h>

struct ComponentTypeInfo {
    size_t elementSize;
    size_t alignment;
};

class ComponentRegistry {
public:
    void registerType(uint32_t hashId, size_t elementSize, size_t alignment = 16);
    const ComponentTypeInfo* getTypeInfo(uint32_t hashId) const;
    bool hasType(uint32_t hashId) const;
    size_t getElementSize(uint32_t hashId) const;
    size_t getAlignment(uint32_t hashId) const;
private:
    ankerl::unordered_dense::map<uint32_t, ComponentTypeInfo, IdentityHash> types;
};
