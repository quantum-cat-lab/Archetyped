#pragma once
#include <cstdlib>
#include <malloc.h> 

inline void* fh_aligned_alloc(std::size_t alignment, std::size_t size) {
#if defined(_MSC_VER)

    return _aligned_malloc(size, alignment);
#else

    return std::aligned_alloc(alignment, size);
#endif
}

inline void fh_aligned_free(void* ptr) {
#if defined(_MSC_VER)
    _aligned_free(ptr);
#else
    std::free(ptr);
#endif
}
