#pragma once
#include "engine/components/ECS/Entity/entity.h"
#include <vector>
#include <cstring>
#include <cstddef>
#include <cstdint>
#include <new>
#include <stdexcept>
#include <cassert>
#include <algorithm>
#include <iostream>
#include "engine/components/MemoryAlloc/EngineArena.h"
#include "engine/components/MemoryAlloc/MemoryAlloc.h"

struct ComponentData {
    static size_t align_up(size_t size, size_t alignment) {
        return (size + alignment - 1) & ~(alignment - 1);
    }
    static constexpr size_t PAGE_SIZE = 1024;
    size_t groupedCount = 0;
    void* data = nullptr;
    size_t elementSize = 0;
    size_t capacity = 0;
    std::vector<uint32_t, ArenaAllocator<uint32_t>> dense;
    std::vector<uint32_t, ArenaAllocator<uint32_t>> denseIndex;
    std::vector<uint32_t*> pages;
    EngineArena* arena_ptr;

    ComponentData(size_t elemSize, size_t cap, EngineArena* arena)
        : capacity(cap), 
        dense(ArenaAllocator<uint32_t>(arena)),
        denseIndex(ArenaAllocator<uint32_t>(arena)),
        arena_ptr(arena){
        elementSize = align_up(elemSize, 16);

        data = arena_ptr->allocate(elementSize * capacity, 64);
        dense.reserve(cap);
        denseIndex.resize(cap, 0xFFFFFFFF);
    }

    uint32_t getSparse(uint32_t id) {
        uint32_t pageIdx = id / PAGE_SIZE;
        uint32_t offset = id % PAGE_SIZE;
        if (pageIdx >= pages.size() || pages[pageIdx] == nullptr) return 0xFFFFFFFF;
        return pages[pageIdx][offset];
    }

    void setSparse(uint32_t id, uint32_t value) {
        uint32_t pageIdx = id / PAGE_SIZE;
        uint32_t offset = id % PAGE_SIZE;
        if (pageIdx >= pages.size()) {
            pages.resize(pageIdx + 1, nullptr);
        }
        if (pages[pageIdx] == nullptr) {
            uint32_t* page = static_cast<uint32_t*>(arena_ptr->allocate(PAGE_SIZE * sizeof(uint32_t), alignof(uint32_t)));
            std::memset(page, 0xFF, PAGE_SIZE * sizeof(uint32_t));
            pages[pageIdx] = page;
        }
        pages[pageIdx][offset] = value;
    }

    void resize(size_t newCapacity) {
        if (newCapacity <= capacity) return;
        
        void* newData = arena_ptr->allocate(elementSize * newCapacity, alignof(std::max_align_t));
        
        for (size_t i = 0; i < dense.size(); ++i) {
            std::memcpy(static_cast<char*>(newData) + elementSize * i, 
                        static_cast<char*>(data) + elementSize * i, 
                        elementSize);
        }
        
        data = newData;
        capacity = newCapacity;
        
        for (size_t i = 0; i < dense.size(); ++i) {
            setSparse(dense[i], static_cast<uint32_t>(i));
        }

        size_t newIndexSize = std::max(newCapacity, denseIndex.size());
        denseIndex.resize(newIndexSize, 0xFFFFFFFF);
    }

    ~ComponentData() = default;
    
    void* getRawPtr() { return data; }
    uint32_t* getDenseIndexPtr() { return denseIndex.data(); }

    void attach(Entity e, void* elem) {
        if (dense.size() >= capacity) {
            resize(capacity * 2);
        }
        if (e.id >= denseIndex.size()) {
            denseIndex.resize(e.id + 1, 0xFFFFFFFF);
        }
        uint32_t existingSparse = getSparse(e.id);
        if (existingSparse != 0xFFFFFFFF) {
            return;
        }

        uint32_t idx = static_cast<uint32_t>(dense.size());
        setSparse(e.id, idx);
        dense.push_back(e.id);
        denseIndex[e.id] = idx;
        std::memcpy(static_cast<char*>(data) + elementSize * idx, elem, elementSize);
        validateDenseSparseConsistency("after attach");
    }
    void swapEntities(uint32_t idx1, uint32_t idx2, char* scratchBuffer){
        if (idx1 == idx2) return;

        uint32_t ent1 = dense[idx1];
        uint32_t ent2 = dense[idx2];
        dense[idx1] = ent2;
        dense[idx2] = ent1;
        setSparse(ent1, idx2);
        setSparse(ent2, idx1);
        denseIndex[ent1] = idx2;
        denseIndex[ent2] = idx1;
        
        char* ptr1 = static_cast<char*>(data) + (idx1 * elementSize);
        char* ptr2 = static_cast<char*>(data) + (idx2 * elementSize);
        
        char stackTemp[256];
        void* temp = nullptr;
        
        if (elementSize <= 256) {
            temp = stackTemp;
        } else {
            temp = scratchBuffer;
        }
        
        std::memcpy(temp, ptr1, elementSize);
        std::memcpy(ptr1, ptr2, elementSize);
        std::memcpy(ptr2, temp, elementSize);
    }
    void moveToGroup(Entity e, char* scratchBuffer) {
        uint32_t currentIdx = getSparse(e.id);
        if (currentIdx < groupedCount) return;

        swapEntities(currentIdx, groupedCount, scratchBuffer);
        groupedCount++;
    }

    void removeFromGroup(Entity e, char* scratchBuffer) {
        uint32_t currentIdx = getSparse(e.id);
        if (currentIdx >= groupedCount) return;

        groupedCount--;
        swapEntities(currentIdx, groupedCount, scratchBuffer);
    }
    void* get(Entity e) {
        assert(e.id < denseIndex.size() && "Entity ID exceeds denseIndex size");
        uint32_t idx = getSparse(e.id);
        assert(idx != 0xFFFFFFFF && "Entity does not have component");
        return static_cast<char*>(data) + elementSize * idx;
    }

    void remove(Entity e) {
        assert(e.id < denseIndex.size() && "Entity ID exceeds denseIndex size");
        uint32_t idx = getSparse(e.id);
        if (idx == 0xFFFFFFFF) return;

        if (idx >= dense.size()) {
            std::cerr << "[ComponentData::remove] ERROR: idx=" << idx << " >= dense.size=" << dense.size() << " for entity " << e.id << std::endl;
            setSparse(e.id, 0xFFFFFFFF);
            denseIndex[e.id] = 0xFFFFFFFF;
            return;
        }

        uint32_t lastEid = dense.back();
        uint32_t lastSparse = getSparse(lastEid);

        if (lastEid == e.id) {
            dense.pop_back();
            setSparse(e.id, 0xFFFFFFFF);
            denseIndex[e.id] = 0xFFFFFFFF;
            return;
        }

        if (lastSparse == 0xFFFFFFFF) {
            std::cerr << "[ComponentData::remove] WARNING: orphan entity " << lastEid << " at back of dense, popping" << std::endl;
            dense.pop_back();
            denseIndex[lastEid] = 0xFFFFFFFF;
            remove(e);
            return;
        }

        std::memcpy(static_cast<char*>(data) + elementSize * idx,
                    static_cast<char*>(data) + elementSize * lastSparse,
                    elementSize);
        dense[idx] = lastEid;
        setSparse(lastEid, idx);
        denseIndex[lastEid] = idx;

        dense.pop_back();
        setSparse(e.id, 0xFFFFFFFF);
        denseIndex[e.id] = 0xFFFFFFFF;
    }

    void validateDenseSparseConsistency(const char* context) {
        for (size_t i = 0; i < dense.size(); ++i) {
            uint32_t eid = dense[i];
            uint32_t sparse = getSparse(eid);
            if (sparse != static_cast<uint32_t>(i)) {
                std::cerr << "[ComponentData::INCONSISTENCY] " << context
                          << " dense[" << i << "]=" << eid
                          << " getSparse(" << eid << ")=" << sparse
                          << " expected=" << i << std::endl;
            }
        }
    }

    bool has(Entity e) {
        bool idOk = e.id < denseIndex.size();
        uint32_t sparse = idOk ? getSparse(e.id) : 0xFFFFFFFF;
        return idOk && sparse != 0xFFFFFFFF;
    }
};
