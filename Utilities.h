#pragma once
#include "Defines.h"
#include <atomic>
#include <cstdint>
#include <stdlib.h>
#include <iostream> // for allocator internal state print-out
#include <utility>  // std::pair

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

// Used by the BuddyAllocator to manage its free Superblocks
struct Superblock : SuperblockHeader {
    Superblock* prev;
    Superblock* next;
};

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
    void vassert_impl(const char* expr, const char* function, const char* file, const unsigned line);
    #define vassert(expr) ((void)(!!(expr) || (andi::vassert_impl(#expr, __FUNCTION__, __FILE__, __LINE__), 0)))
#else
    #define vassert(expr) ((void)0)
#endif // HPC_DEBUG
}

// Helper math functions
uint32_t min(uint32_t, uint32_t);
uint32_t max(uint32_t, uint32_t);
uint64_t max(uint64_t, uint64_t);
uint32_t leastSetBit(uint32_t);
uint32_t leastSetBit(uint64_t);
uint32_t fastlog2(uint32_t);
uint32_t fastlog2(uint64_t);
