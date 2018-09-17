#pragma once
#include <malloc.h>
#include <stdlib.h>
#include <atomic>
#include <cstdint>
#include <iostream> // for allocator internal state print-out

// These definitions control the allocator behaviour (see README.md)
#define HPC_DEBUG 1
#define USE_SMALL_POOLS 1

enum Constants : size_t {
    // Minimum alignment for allocation requests
    Alignment = 32,
    // Logarithm of the buddy allocator address space size in bytes
    K = (sizeof(void*) == 8) ? 31 : 29,
    // The buddy allocators need to have their size a power of 2:
    // 2GB in 64-bit mode, 512MB in 32-bit
    BuddyAllocatorSize = size_t(1) << K,
    // Invalid block index for the small pools
    InvalidIdx = ~size_t(0),
    // Logarithm of the smallest allocation size, in bytes
    MinAllocationSizeLog = 5,
    // Minimum allocation size, in bytes
    MinAllocationSize = size_t(1) << MinAllocationSizeLog,
    // Number of blocks in the fixed-size pools:
    SmallPoolSize0 = 1'500'000, //   32B
    SmallPoolSize1 = 1'500'000, //   64B
    SmallPoolSize2 =   500'000, //  128B
    SmallPoolSize3 =   250'000, //  256B
    SmallPoolSize4 =   200'000, //  512B
    SmallPoolSize5 =   200'000, // 1024B
};
// Scoped enums are nice, but require overly verbose conversions to the underlying type...

namespace andi
{
    void* aligned_malloc(size_t);
    void aligned_free(void*);

    // A small busy-waiting mutex - replaces the cost of context switching with
    // that of a thread staying alive, hoping the wait does not take long
    // (in which case the total processing time will increase)
    class mutex {
        std::atomic<bool> locked;
    public:
        mutex() : locked{ false } {}
        void lock() {
            bool isLocked = false;
            while (!locked.compare_exchange_strong(isLocked, true))
                isLocked = false;
        }
        void unlock() { locked = false; }
    };

    class lock_guard {
        mutex& mtx;
    public:
        lock_guard(mutex& mtx) : mtx{ mtx } { mtx.lock(); }
        ~lock_guard() { mtx.unlock(); }
    };

#if HPC_DEBUG == 1
    void vassert_impl(const char* expr, const char* file, const unsigned line);
    #define vassert(expr) ((void)(!!(expr) || (andi::vassert_impl(#expr, __FILE__, __LINE__), 0)))
#else
    #define vassert(expr) ((void)0)
#endif // HPC_DEBUG
}
