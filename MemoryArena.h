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
    SmallPool<32, sizes[0]> tp0;
    SmallPool<64, sizes[1]> tp1;
    SmallPool<128, sizes[2]> tp2;
    SmallPool<256, sizes[3]> tp3;
    SmallPool<512, sizes[4]> tp4;
    SmallPool<1024, sizes[5]> tp5;
#endif // USE_SMALL_POOLS

    MemoryPool largePool[2];
    std::atomic_uint32_t toggle;
    andi::mutex initializationmtx;
    bool _isInitialized;

    // look-up "static initialization fiasco"
    static MemoryArena arena;

    MemoryArena();
    static size_t max_size() noexcept;
    static void LockAll() noexcept;
    static void UnlockAll() noexcept;
    static bool isInside(void*) noexcept;

public:
    // moving or copying of arenas is forbidden
    MemoryArena(const MemoryArena&) = delete;
    MemoryArena& operator=(const MemoryArena&) = delete;
    MemoryArena(MemoryArena&&) = delete;
    MemoryArena& operator=(MemoryArena&&) = delete;

    static bool Initialize() noexcept(isRelease);
    static bool Deinitialize() noexcept(isRelease);
    static bool isInitialized() noexcept;
    static void* Allocate(size_t) noexcept(isRelease);
    static void Deallocate(void*) noexcept(isRelease);
    // A very helpful methos to print the buddy allocator's state
    static void printCondition();
};
