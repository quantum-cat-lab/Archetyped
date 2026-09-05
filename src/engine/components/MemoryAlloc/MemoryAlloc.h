#pragma once
#include "alloc_utils.h"
#include "EngineArena.h"
#include <limits>
#include <memory>
#include <utility>

/**
 * ArenaAllocator (Arena Allocator) - Core Component.
 * A STL-compatible allocator that uses an EngineArena for memory allocation.
 */
template<typename T>
class ArenaAllocator {
public:
    using value_type = T;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;

    EngineArena* arena;
    /**
     * Initializes the allocator with a specific memory arena.
     * 
     * @param a Pointer to the EngineArena to use for allocations.
     */
    ArenaAllocator(EngineArena* a) : arena(a) {}
    template<typename U>
    ArenaAllocator(const ArenaAllocator<U>& other) : arena(other.arena) {}

    /**
     * Allocates memory for n elements of type T.
     * 
     * @param n Number of elements to allocate.
     * @return Pointer to the allocated memory.
     */
    T* allocate(size_t n) {
        return static_cast<T*>(arena->allocate(n * sizeof(T), alignof(T)));
    }
    /**
     * Deallocates memory. Note that ArenaAllocator typically does not free individual allocations.
     * 
     * @param ptr Pointer to the memory to deallocate.
     * @param n Number of elements that were allocated.
     */
    void deallocate(T* ptr, size_t n) {
    }

    /**
     * Returns the maximum size of an object that can be allocated.
     * 
     * @return The maximum possible number of elements.
     */
    size_t max_size() const {
        return std::numeric_limits<size_t>::max() / sizeof(T);
    }

    template<typename U>
    struct rebind {
        using other = ArenaAllocator<U>;
    };

    /**
     * Returns the address of the given reference.
     * 
     * @param x The reference to get the address of.
     * @return The pointer to the referenced object.
     */
    pointer address(reference x) const { return &x; }
    /**
     * Returns the address of the given constant reference.
     * 
     * @param x The constant reference to get the address of.
     * @return The constant pointer to the referenced object.
     */
    const_pointer address(const_reference x) const { return &x; }

    /**
     * Constructs an object in the allocated storage.
     * 
     * @param p Pointer to the allocated storage.
     * @param val The value to use for construction.
     */
    void construct(pointer p, const_reference val) {
        ::new (p) T(val);
    }
    /**
     * Destroys an object in the allocated storage.
     * 
     * @param p Pointer to the object to destroy.
     */
    void destroy(pointer p) {
        p->~T();
    }
    /**
     * Constructs an object of type U in the allocated storage.
     * 
     * @param p Pointer to the allocated storage.
     * @param args Arguments to forward to the constructor of U.
     */
    template<typename U, typename... Args>
    void construct(U* p, Args&&... args) {
        ::new (p) U(std::forward<Args>(args)...);
    }

    /**
     * Checks if two allocators are equal.
     * 
     * @param other The other allocator to compare with.
     * @return true if they use the same arena, false otherwise.
     */
    bool operator==(const ArenaAllocator& other) const {
        return arena == other.arena;
    }
    /**
     * Checks if two allocators are not equal.
     * 
     * @param other The other allocator to compare with.
     * @return true if they use different arenas, false otherwise.
     */
    bool operator!=(const ArenaAllocator& other) const {
        return arena != other.arena;
    }
    /**
     * Returns an allocator for the same arena to be used during container copy construction.
     * 
     * @return A new ArenaAllocator instance sharing the same arena.
     */
    ArenaAllocator select_on_container_copy_construction() const {
        return ArenaAllocator(arena);
    }

    /**
     * Retrieves the arena associated with this allocator.
     * 
     * @return Pointer to the EngineArena.
     */
    EngineArena* getArena() const { return arena; }
    /**
     * Sets the arena associated with this allocator.
     * 
     * @param a Pointer to the new EngineArena.
     */
    void setArena(EngineArena* a) { arena = a; }
};
