#pragma once
#include "SmallPool.h"
#include "MemoryPool.h"

// Forward declaration of the allocator, which can access the arena's methods. Of course, memory
// can always be allocated & deallocated using MemoryArena::Allocate() and MemoryArena::Deallocate()
namespace andi { template<class> class allocator; }

// The MemoryArena is a singleton (!) and all memory operations go through it.
// It manages several memory pools and is the only one that can access them directly.
class MemoryArena {
    template<class> friend class andi::allocator;

#if USE_SMALL_POOLS == 1
    SmallPool<  32, Constants::SmallPoolSize0> tp0;
    SmallPool<  64, Constants::SmallPoolSize1> tp1;
    SmallPool< 128, Constants::SmallPoolSize2> tp2;
    SmallPool< 256, Constants::SmallPoolSize3> tp3;
    SmallPool< 512, Constants::SmallPoolSize4> tp4;
    SmallPool<1024, Constants::SmallPoolSize5> tp5;
#endif // USE_SMALL_POOLS

    MemoryPool largePool[2];
    std::atomic<uint32_t> toggle;
    andi::mutex initializationmtx;
    std::atomic<bool> _isInitialized;

    // look-up "static initialization fiasco"
    static MemoryArena arena;

    MemoryArena();
    static size_t max_size();
    static void LockAll();
    static void UnlockAll();
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
