#include "MemoryArena.h"

MemoryArena MemoryArena::arena{};

MemoryArena::MemoryArena() : _isInitialized(false) {}

bool MemoryArena::Initialize() {
    andi::lock_guard lock{ arena.initializationmtx };
    if (arena._isInitialized) {
        vassert(false && "MemoryArena has already been initialized!");
        return false;
    }

#if USE_SMALL_POOLS == 1
    arena.tp0.Initialize();
    arena.tp1.Initialize();
    arena.tp2.Initialize();
    arena.tp3.Initialize();
    arena.tp4.Initialize();
    arena.tp5.Initialize();
#endif // USE_SMALL_POOLS
    
    arena.largePool[0].Initialize();
    arena.largePool[1].Initialize();
    arena._isInitialized = true;
    return true;
}

bool MemoryArena::Deinitialize() {
    andi::lock_guard lock{ arena.initializationmtx };
    if (!arena._isInitialized) {
        vassert(false && "MemoryArena has already been deinitialized!");
        return false;
    }

#if USE_SMALL_POOLS == 1
    arena.tp0.Deinitialize();
    arena.tp1.Deinitialize();
    arena.tp2.Deinitialize();
    arena.tp3.Deinitialize();
    arena.tp4.Deinitialize();
    arena.tp5.Deinitialize();
#endif // USE_SMALL_POOLS
    
    arena.largePool[0].Deinitialize();
    arena.largePool[1].Deinitialize();
    arena._isInitialized = false;
    return true;
}

bool MemoryArena::isInitialized() {
    return arena._isInitialized;
}

void* MemoryArena::Allocate(size_t n) {
    if (n == 0)
        return nullptr;
    vassert(arena._isInitialized && "MemoryArena must be initialized before allocation!\n");

    void* ptr = nullptr;
#if USE_SMALL_POOLS == 1
    if (n <= 32)
        ptr = arena.tp0.Allocate();
    else if (n <= 64)
        ptr = arena.tp1.Allocate();
    else if (n <= 128)
        ptr = arena.tp2.Allocate();
    else if (n <= 256)
        ptr = arena.tp3.Allocate();
    else if (n <= 512)
        ptr = arena.tp4.Allocate();
    else if (n <= 1024)
        ptr = arena.tp5.Allocate();
    // In case allocation has been unsuccessful due to a full memory pool
    if (ptr == nullptr) {
#endif // USE_SMALL_POOLS
        const uint32_t idx = arena.toggle.fetch_add(1) & 1;
        ptr = arena.largePool[idx].Allocate(n);
#if USE_SMALL_POOLS == 1
    }
#endif // USE_SMALL_POOLS
    return ptr;
}

void MemoryArena::Deallocate(void* ptr) {
    if (!ptr)
        return;
    vassert(arena._isInitialized && "MemoryArena must be initialized before deallocation!\n");
    vassert(arena.isInside(ptr) && "MemoryArena: pointer is outside of the address space!\n");

#if USE_SMALL_POOLS == 1
    if (arena.tp0.isInside(ptr))
        arena.tp0.Deallocate(ptr);
    else if (arena.tp1.isInside(ptr))
        arena.tp1.Deallocate(ptr);
    else if (arena.tp2.isInside(ptr))
        arena.tp2.Deallocate(ptr);
    else if (arena.tp3.isInside(ptr))
        arena.tp3.Deallocate(ptr);
    else if (arena.tp4.isInside(ptr))
        arena.tp4.Deallocate(ptr);
    else if (arena.tp5.isInside(ptr))
        arena.tp5.Deallocate(ptr);
    else
#endif // USE_SMALL_POOLS
    if (arena.largePool[0].isInside(ptr))
        arena.largePool[0].Deallocate(ptr);
    else
        arena.largePool[1].Deallocate(ptr);
}

void MemoryArena::printCondition() {
#if USE_SMALL_POOLS == 1
    arena.tp0.printCondition();
    arena.tp1.printCondition();
    arena.tp2.printCondition();
    arena.tp3.printCondition();
    arena.tp4.printCondition();
    arena.tp5.printCondition();
#endif // USE_SMALL_POOLS

    arena.largePool[0].printCondition();
    arena.largePool[1].printCondition();
}

size_t MemoryArena::max_size() {
    return MemoryPool::max_size();
}

bool MemoryArena::isInside(void* ptr) {
    return (
#if USE_SMALL_POOLS == 1
            arena.tp0.isInside(ptr) || arena.tp1.isInside(ptr) ||
            arena.tp2.isInside(ptr) || arena.tp3.isInside(ptr) ||
            arena.tp4.isInside(ptr) || arena.tp5.isInside(ptr) ||
#endif // USE_SMALL_POOLS
            arena.largePool[0].isInside(ptr) || arena.largePool[1].isInside(ptr));
}

// iei
