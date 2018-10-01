#pragma once
#include <malloc.h>
#include <stdlib.h>
#include <atomic>
#include <cstdint>
#include <iostream> // for allocator internal state print-out
#include <utility>  // std::pair

// These definitions control the allocator behaviour (see README.md)
#define HPC_DEBUG 1
#define USE_POOL_ALLOCATORS 0

// Each BuddyAllocator allocation needs the following header to manage the allocations.
// In theory this header can be reduced to 7 bits (!) -> O(lglgn)
struct SuperblockHeader {
#if HPC_DEBUG == 1
    union {
        struct {
            uint16_t k;
            uint16_t free;
        };
        uint32_t blueprint;
    };
    uint32_t signature;
#else
    uint32_t k;
    uint32_t free;
#endif // HPC_DEBUG
};

enum Constants : size_t {
    // Minimum alignment for allocation requests
    Alignment = 32,
    // Logarithm of the buddy allocator address space size in bytes
    K = (sizeof(void*) == 8) ? 31 : 29,
    // The buddy allocators need to have their size a power of 2:
    // 2GB in 64-bit mode, 512MB in 32-bit
    BuddyAllocatorSize = size_t(1) << K,
    // Superblock header size, in bytes
    HeaderSize = sizeof(SuperblockHeader),
    // Invalid block index for the small pools
    InvalidIdx = ~size_t(0),
    // Logarithm of the smallest allocation size, in bytes
    MinAllocationSizeLog = 5,
    // Minimum allocation size, in bytes
    MinAllocationSize = size_t(1) << MinAllocationSizeLog,
    // The upper limit for a single allocation
    MaxAllocationSize = (Constants::BuddyAllocatorSize / 4) - Constants::HeaderSize,
    // Number of blocks in the fixed-size pools:
    PoolSize0 = 1'500'000, //   32B
    PoolSize1 = 1'500'000, //   64B
    PoolSize2 =   500'000, //  128B
    PoolSize3 =   250'000, //  256B
    PoolSize4 =   200'000, //  512B
    PoolSize5 =   200'000, // 1024B
};
// Scoped enums are nice, but require overly verbose conversions to the underlying type...

// Used by the BuddyAllocator to manage its free Superblocks
struct Superblock : SuperblockHeader {
    Superblock* prev;
    Superblock* next;
};

// Sanity checks for global constants' validity
static_assert(Constants::HeaderSize < Constants::Alignment);
static_assert(Constants::Alignment % alignof(Superblock) == 0); // virtualZero should be a valid Superblock address
static_assert(Constants::K <= 63); // we want (2^largePoolSizeLog) to fit in 64 bits
static_assert(Constants::HeaderSize < Constants::MinAllocationSize); // otherwise headers overlap and mayhem ensues
static_assert(Constants::MinAllocationSizeLog >= 5
           && Constants::MinAllocationSizeLog <= Constants::K);
static_assert(Constants::MaxAllocationSize <= Constants::BuddyAllocatorSize
           && Constants::MaxAllocationSize + Constants::HeaderSize <= 0x1'0000'0000ui64);
static_assert(offsetof(Superblock, prev) == Constants::HeaderSize); // fun fact: this causes undefined behaviour

namespace andi
{
    void* aligned_malloc(size_t);
    void aligned_free(void*);

    // A small busy-waiting mutex - replaces the cost of context switching with
    // that of a thread staying alive, hoping the wait does not take long
    // (in which case the total processing time will increase)
    class mutex {
        friend class lock_guard;
        std::atomic<bool> locked;

        void lock() {
            bool isLocked = false;
            while (!locked.compare_exchange_strong(isLocked, true))
                isLocked = false;
        }
        void unlock() { locked = false; }
    public:
        mutex() : locked{ false } {}
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
