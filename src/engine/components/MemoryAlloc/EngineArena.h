#pragma once
#include "alloc_utils.h"
#include <cstdint>
#include <memory.h>
#include <memory>
#include <stdexcept>
#include <stdlib.h>
#include <vector>

class EngineArena {
    struct Block {
        uint8_t* ptr;
        size_t size;
    };
    std::vector<Block> blocks;
    size_t blockSize;
    size_t offset;
    uint8_t *currentBlock;

    void allocateNewBlock() {
        size_t alignedSize = (blockSize + 63) & ~63;
        uint8_t *b = static_cast<uint8_t *>(fh_aligned_alloc(64, alignedSize));
        if (!b)
            throw std::bad_alloc();
        blocks.push_back({b, alignedSize});
        currentBlock = b;
        offset = 0;
    }

public:
    EngineArena(size_t size) : blockSize(size), offset(0) { allocateNewBlock(); }
    ~EngineArena() {
        for (auto& b : blocks) {
            fh_aligned_free(b.ptr);
        }
    }

    void *allocate(size_t size, size_t alignment = 16) {
        if (size > blockSize) {
            uint8_t *b = static_cast<uint8_t *>(fh_aligned_alloc(alignment, size));
            if (!b)
                throw std::bad_alloc();
            blocks.push_back({b, size});
            return b;
        }

        uintptr_t current_addr = reinterpret_cast<uintptr_t>(currentBlock + offset);
        size_t padding = (alignment - (current_addr % alignment)) % alignment;
        if (offset + padding + size > blockSize) {
            allocateNewBlock();
            current_addr = reinterpret_cast<uintptr_t>(currentBlock + offset);
            padding = (alignment - (current_addr % alignment)) % alignment;
        }
        offset += padding;
        void *ptr = currentBlock + offset;
        offset += size;
        return ptr;
    }

    void reset() {
        offset = 0;
        if (!blocks.empty()) {
            currentBlock = blocks[0].ptr;
            if (blocks.size() > 1) {
                for (size_t i = 1; i < blocks.size(); ++i) {
                    fh_aligned_free(blocks[i].ptr);
                }
                blocks.resize(1);
            }
        }
    }

    size_t getUsedSize() const {
        size_t total = 0;
        for (const auto& b : blocks) {
            if (b.ptr == currentBlock) {
                total += offset;
            } else {
                total += b.size;
            }
        }
        return total;
    }

    size_t getRemainingSize() const {
        if (!currentBlock) return 0;
        return ((blockSize + 63) & ~63) - offset;
    }

    size_t getBlockSize() const { return blockSize; }
    size_t getBlockCount() const { return blocks.size(); }

    bool owns(const void* ptr) const {
        for (const auto& b : blocks) {
            if (ptr >= b.ptr && ptr < b.ptr + b.size) return true;
        }
        return false;
    }

    void release() {
        for (auto& b : blocks) {
            fh_aligned_free(b.ptr);
        }
        blocks.clear();
        offset = 0;
        currentBlock = nullptr;
        allocateNewBlock();
    }

    void reserve(size_t size) {
        if (getRemainingSize() < size) {
            if (size > blockSize) {
                setBlockSize(size);
            }
            allocateNewBlock();
        }
    }

    void setBlockSize(size_t size) { blockSize = size; }
    void* getCurrentBlock() const { return currentBlock; }
    size_t getCurrentOffset() const { return offset; }

    void copyFrom(const EngineArena& other) {
        for (const auto& b : other.blocks) {
            void* ptr = allocate(b.size);
            memcpy(ptr, b.ptr, b.size);
        }
    }
};
