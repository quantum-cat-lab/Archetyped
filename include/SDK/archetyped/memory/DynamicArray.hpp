#pragma once

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <type_traits>

struct MallocAllocator {
    static void* allocate(size_t size) {
        return malloc(size);
    }

    static void* reallocate(void* ptr, size_t size) {
        return realloc(ptr, size);
    }

    static void deallocate(void* ptr) noexcept {
        free(ptr);
    }
};

template<typename T, typename Allocator = MallocAllocator>
struct DynamicArray {
    static_assert(std::is_trivially_copyable_v<T>,
                  "DynamicArray requires trivially copyable types");

    T*        data;
    uint16_t  count;
    uint16_t  capacity;
};

template<typename T, typename Allocator>
inline void da_init(DynamicArray<T, Allocator>* arr, uint16_t init_capacity) {
    arr->count    = 0;
    arr->capacity = init_capacity;
    arr->data     = nullptr;

    if (init_capacity > 0) {
        arr->data = static_cast<T*>(Allocator::allocate(init_capacity * sizeof(T)));
        if (arr->data == nullptr) {
            fprintf(stderr, "Critical: DynamicArray allocation failed during init!\n");
            exit(EXIT_FAILURE);
        }
    }
}

template<typename T, typename Allocator>
inline void da_reserve(DynamicArray<T, Allocator>* arr, uint16_t new_capacity) {
    if (new_capacity <= arr->capacity) {
        return;
    }

    T* temp = static_cast<T*>(Allocator::reallocate(arr->data, new_capacity * sizeof(T)));
    if (temp == nullptr) {
        fprintf(stderr, "Critical: DynamicArray reallocation failed!\n");
        exit(EXIT_FAILURE);
    }

    arr->data     = temp;
    arr->capacity = new_capacity;
}

template<typename T, typename Allocator>
inline uint16_t da_push(DynamicArray<T, Allocator>* arr, T value) {
    if (arr->count >= arr->capacity) {
        uint32_t new_capacity = (arr->capacity == 0) ? 4 : static_cast<uint32_t>(arr->capacity) * 2;
        if (new_capacity > UINT16_MAX) {
            new_capacity = UINT16_MAX;
        }
        if (new_capacity <= arr->capacity) {
            fprintf(stderr, "Error: DynamicArray at maximum capacity (%u)\n", UINT16_MAX);
            return UINT16_MAX;
        }
        da_reserve(arr, static_cast<uint16_t>(new_capacity));
    }

    uint16_t index = arr->count;
    arr->data[index] = value;
    arr->count++;
    return index;
}

template<typename T, typename Allocator>
inline void da_pop(DynamicArray<T, Allocator>* arr) {
    if (arr->count > 0) {
        arr->count--;
    }
}

template<typename T, typename Allocator>
inline void da_free(DynamicArray<T, Allocator>* arr) {
    if (arr->data != nullptr) {
        Allocator::deallocate(arr->data);
        arr->data = nullptr;
    }
    arr->count    = 0;
    arr->capacity = 0;
}

template<typename T, typename Allocator>
inline T* da_get(const DynamicArray<T, Allocator>* arr, uint16_t index) {
    if (index >= arr->count) {
        fprintf(stderr, "Error: Index %u out of bounds (Size: %u)\n", index, arr->count);
        return nullptr;
    }
    return &arr->data[index];
}

template<typename T, typename Allocator>
inline void da_remove(DynamicArray<T, Allocator>* arr, uint16_t index) {
    if (index >= arr->count) {
        fprintf(stderr, "Error: Cannot remove, index %u out of bounds\n", index);
        return;
    }

    if (index < arr->count - 1) {
        size_t elements_to_move = arr->count - index - 1;
        memmove(&arr->data[index], &arr->data[index + 1], elements_to_move * sizeof(T));
    }

    arr->count--;
}

template<typename T, typename Allocator>
inline void da_fast_remove(DynamicArray<T, Allocator>* arr, uint16_t index) {
    if (index >= arr->count) {
        fprintf(stderr, "Error: Cannot fast-remove, index %u out of bounds\n", index);
        return;
    }

    if (index < arr->count - 1) {
        arr->data[index] = arr->data[arr->count - 1];
    }

    arr->count--;
}

template<typename T, typename Allocator>
inline void da_clear(DynamicArray<T, Allocator>* arr) {
    arr->count = 0;
}

template<typename T, typename Allocator>
inline bool da_empty(const DynamicArray<T, Allocator>* arr) {
    return arr->count == 0;
}

template<typename T, typename Allocator>
inline uint16_t da_size(const DynamicArray<T, Allocator>* arr) {
    return arr->count;
}

template<typename T, typename Allocator, typename Pred>
inline int64_t da_find(const DynamicArray<T, Allocator>* arr, Pred&& pred) {
    for (uint16_t i = 0; i < arr->count; ++i) {
        if (pred(arr->data[i])) {
            return static_cast<int64_t>(i);
        }
    }
    return -1;
}

template<typename T, typename Allocator>
inline bool da_contains(const DynamicArray<T, Allocator>* arr, const T& value) {
    return da_find(arr, [&value](const T& v) { return v == value; }) != -1;
}
