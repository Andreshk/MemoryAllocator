﻿#include "Commons.h"

void* andi::aligned_malloc(size_t size, size_t alignment) noexcept {
#if defined(_MSC_VER)
    return _aligned_malloc(size, alignment);
#else
    void* ptr;
    posix_memalign(&ptr, alignment, size);
    return ptr;
#endif
}

void andi::aligned_free(void* ptr) noexcept {
#if defined(_MSC_VER)
    _aligned_free(ptr);
#else
    free(ptr);
#endif
}

void andi::mutex::lock() noexcept {
    bool isLocked = false;
    while (!locked.compare_exchange_strong(isLocked, true))
        isLocked = false;
}

void andi::mutex::unlock() noexcept {
    locked = false;
}

// iei
