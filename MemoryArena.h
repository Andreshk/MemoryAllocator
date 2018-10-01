﻿#pragma once
#include "PoolAllocator.h"
#include "BuddyAllocator.h"

// Forward declaration of the allocator, which can access the arena's methods. Of course, memory
// can always be allocated & deallocated using MemoryArena::Allocate() and MemoryArena::Deallocate()
namespace andi { template<class> class allocator; }

// The MemoryArena is a singleton (!) and all memory operations go through it.
// It manages several memory pools and is the only one that can access them directly.
class MemoryArena {
    template<class> friend class andi::allocator;

#if USE_POOL_ALLOCATORS == 1
    PoolAllocator<  32, Constants::PoolSize0> pool0;
    PoolAllocator<  64, Constants::PoolSize1> pool1;
    PoolAllocator< 128, Constants::PoolSize2> pool2;
    PoolAllocator< 256, Constants::PoolSize3> pool3;
    PoolAllocator< 512, Constants::PoolSize4> pool4;
    PoolAllocator<1024, Constants::PoolSize5> pool5;
#endif // USE_POOL_ALLOCATORS

    BuddyAllocator buddyAlloc[2];
    std::atomic<uint32_t> toggle;
    andi::mutex initializationmtx;
    std::atomic<bool> initialized;

    // look-up "static initialization fiasco"
    static MemoryArena arena;

    MemoryArena();
    static size_t max_size();
    static bool isInside(void*);

public:
    // moving or copying of arenas is forbidden
    MemoryArena(const MemoryArena&) = delete;
    MemoryArena& operator=(const MemoryArena&) = delete;
    MemoryArena(MemoryArena&&) = delete;
    MemoryArena& operator=(MemoryArena&&) = delete;

    static bool Initialize();
    static bool Deinitialize();
    static bool isInitialized();
    static void* Allocate(size_t);
    static void Deallocate(void*);
    // A very helpful methos to print the buddy allocator's state
    static void printCondition();
};
