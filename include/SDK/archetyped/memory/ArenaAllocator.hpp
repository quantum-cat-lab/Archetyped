#pragma once
#include "alloc_utils.h"
#include "EngineArena.h"
#include <limits>
#include <memory>
#include <utility>

template<typename T>
class ArenaAllocator {
public:
    using value_type = T;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;

    EngineArena* arena;

    ArenaAllocator(EngineArena* a) : arena(a) {}
    template<typename U>
    ArenaAllocator(const ArenaAllocator<U>& other) : arena(other.arena) {}

    T* allocate(size_t n) {
        return static_cast<T*>(arena->allocate(n * sizeof(T), alignof(T)));
    }

    void deallocate(T* ptr, size_t n) {
    }

    size_t max_size() const {
        return std::numeric_limits<size_t>::max() / sizeof(T);
    }

    template<typename U>
    struct rebind {
        using other = ArenaAllocator<U>;
    };

    pointer address(reference x) const { return &x; }
    const_pointer address(const_reference x) const { return &x; }

    void construct(pointer p, const_reference val) {
        ::new (p) T(val);
    }

    void destroy(pointer p) {
        p->~T();
    }

    template<typename U, typename... Args>
    void construct(U* p, Args&&... args) {
        ::new (p) U(std::forward<Args>(args)...);
    }

    bool operator==(const ArenaAllocator& other) const {
        return arena == other.arena;
    }

    bool operator!=(const ArenaAllocator& other) const {
        return arena != other.arena;
    }

    ArenaAllocator select_on_container_copy_construction() const {
        return ArenaAllocator(arena);
    }

    EngineArena* getArena() const { return arena; }
    void setArena(EngineArena* a) { arena = a; }
};
