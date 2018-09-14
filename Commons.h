#pragma once
#include <malloc.h>
#include <stdlib.h>
#include <atomic>
#include <cstdint>
#include <iostream>
#include <exception>

// These definitions control the allocator behaviour (see README.md)
#define HPC_DEBUG 0
#define USE_SMALL_POOLS 0

#if USE_SMALL_POOLS == 1
// Manage the number of blocks in each of the fixed-size pools.
// # of blocks with size:     <=32B   64B     128B   256B   512B   1024B
constexpr size_t sizes[6] = { 1500000,1500000,500000,250000,200000,200000 };
constexpr size_t invalidIdx = ~size_t(0);
#endif // USE_SMALL_POOLS

// The buddy allocators need to have thier size a power of 2
#ifdef NDEBUG
// Logarithm of the address space size in bytes
constexpr uint32_t largePoolSizeLog = (sizeof(void*) == 8) ? 32 : 29;
#else
constexpr uint32_t largePoolSizeLog = 29;
#endif
// 4GB in 64-bit mode, 512MB in 32-bit or for debugging
constexpr size_t largePoolSize = size_t(1) << largePoolSizeLog;

// Functions, declared noexcept(isRelease) may throw only
// when HPC_DEBUG is 1, and are otherwise strictly noexcept.
constexpr bool isRelease = (HPC_DEBUG == 0);

namespace andi
{
    void* aligned_malloc(size_t, size_t) noexcept;
    void aligned_free(void*) noexcept;

    // A small busy-waiting mutex - replaces the cost of context switching with
    // that of a thread staying alive, hoping the wait does not take long
    // (in which case the total processing time will increase)
    class mutex {
        std::atomic<bool> locked;
    public:
        void lock() noexcept;
        void unlock() noexcept;
    };
}
