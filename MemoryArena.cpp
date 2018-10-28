#include "MemoryArena.h"

MemoryArena MemoryArena::arena{};

MemoryArena::MemoryArena() : initialized(false) {}

bool MemoryArena::Initialize() {
    andi::lock_guard lock{ arena.initializationmtx };
    if (arena.initialized) {
        vassert(false && "MemoryArena has already been initialized!");
        return false;
    }

#if USE_POOL_ALLOCATORS == 1
    arena.pool0.Initialize();
    arena.pool1.Initialize();
    arena.pool2.Initialize();
    arena.pool3.Initialize();
    arena.pool4.Initialize();
    arena.pool5.Initialize();
#endif // USE_POOL_ALLOCATORS
    
    arena.buddyAlloc[0].Initialize();
    arena.buddyAlloc[1].Initialize();
    arena.initialized = true;
    return true;
}

bool MemoryArena::Deinitialize() {
    andi::lock_guard lock{ arena.initializationmtx };
    if (!arena.initialized) {
        vassert(false && "MemoryArena has already been deinitialized!");
        return false;
    }

#if USE_POOL_ALLOCATORS == 1
    arena.pool0.Deinitialize();
    arena.pool1.Deinitialize();
    arena.pool2.Deinitialize();
    arena.pool3.Deinitialize();
    arena.pool4.Deinitialize();
    arena.pool5.Deinitialize();
#endif // USE_POOL_ALLOCATORS
    
    arena.buddyAlloc[0].Deinitialize();
    arena.buddyAlloc[1].Deinitialize();
    arena.initialized = false;
    return true;
}

void* MemoryArena::Allocate(size_t n) {
    if (n == 0)
        return nullptr;
    vassert(arena.initialized && "MemoryArena must be initialized before allocation!");

    void* ptr = nullptr;
#if USE_POOL_ALLOCATORS == 1
    if (n <= 32)
        ptr = arena.pool0.Allocate();
    else if (n <= 64)
        ptr = arena.pool1.Allocate();
    else if (n <= 128)
        ptr = arena.pool2.Allocate();
    else if (n <= 256)
        ptr = arena.pool3.Allocate();
    else if (n <= 512)
        ptr = arena.pool4.Allocate();
    else if (n <= 1024)
        ptr = arena.pool5.Allocate();
    // In case allocation has been unsuccessful due to a full memory pool
    if (ptr == nullptr) {
#endif // USE_POOL_ALLOCATORS
        const uint32_t idx = arena.toggle.fetch_add(1) & 1;
        ptr = arena.buddyAlloc[idx].Allocate(n);
#if USE_POOL_ALLOCATORS == 1
    }
#endif // USE_POOL_ALLOCATORS
    vassert(ptr);
    return ptr;
}

void MemoryArena::Deallocate(void* ptr) {
    if (!ptr)
        return;
    vassert(arena.initialized && "MemoryArena must be initialized before deallocation!");
    vassert(arena.Contains(ptr) && "MemoryArena: pointer is outside of the address space!");

#if USE_POOL_ALLOCATORS == 1
    if (arena.pool0.Contains(ptr))
        arena.pool0.Deallocate(ptr);
    else if (arena.pool1.Contains(ptr))
        arena.pool1.Deallocate(ptr);
    else if (arena.pool2.Contains(ptr))
        arena.pool2.Deallocate(ptr);
    else if (arena.pool3.Contains(ptr))
        arena.pool3.Deallocate(ptr);
    else if (arena.pool4.Contains(ptr))
        arena.pool4.Deallocate(ptr);
    else if (arena.pool5.Contains(ptr))
        arena.pool5.Deallocate(ptr);
    else
#endif // USE_POOL_ALLOCATORS
    if (arena.buddyAlloc[0].Contains(ptr))
        arena.buddyAlloc[0].Deallocate(ptr);
    else
        arena.buddyAlloc[1].Deallocate(ptr);
}

std::pair<void*, size_t> MemoryArena::AllocateUseful(size_t n){
    if (n == 0)
        return { nullptr, 0 };
    vassert(arena.initialized && "MemoryArena must be initialized before allocation!");

    std::pair<void*, size_t> res{ nullptr, 0 };
#if USE_POOL_ALLOCATORS == 1
    if (n <= 32)
        res = arena.pool0.AllocateUseful();
    else if (n <= 64)
        res = arena.pool1.AllocateUseful();
    else if (n <= 128)
        res = arena.pool2.AllocateUseful();
    else if (n <= 256)
        res = arena.pool3.AllocateUseful();
    else if (n <= 512)
        res = arena.pool4.AllocateUseful();
    else if (n <= 1024)
        res = arena.pool5.AllocateUseful();
    // In case allocation has been unsuccessful due to a full memory pool
    if (res.first == nullptr) {
#endif // USE_POOL_ALLOCATORS
        const uint32_t idx = arena.toggle.fetch_add(1) & 1;
        res = arena.buddyAlloc[idx].AllocateUseful(n);
#if USE_POOL_ALLOCATORS == 1
    }
#endif // USE_POOL_ALLOCATORS
    return res;
}

void MemoryArena::PrintCondition() {
#if USE_POOL_ALLOCATORS == 1
    arena.pool0.PrintCondition();
    arena.pool1.PrintCondition();
    arena.pool2.PrintCondition();
    arena.pool3.PrintCondition();
    arena.pool4.PrintCondition();
    arena.pool5.PrintCondition();
#endif // USE_POOL_ALLOCATORS

    arena.buddyAlloc[0].PrintCondition();
    arena.buddyAlloc[1].PrintCondition();
}

size_t MemoryArena::MaxSize() {
    return BuddyAllocator::MaxSize();
}

bool MemoryArena::Contains(void* ptr) {
    return (
#if USE_POOL_ALLOCATORS == 1
            arena.pool0.Contains(ptr) || arena.pool1.Contains(ptr) ||
            arena.pool2.Contains(ptr) || arena.pool3.Contains(ptr) ||
            arena.pool4.Contains(ptr) || arena.pool5.Contains(ptr) ||
#endif // USE_POOL_ALLOCATORS
            arena.buddyAlloc[0].Contains(ptr) || arena.buddyAlloc[1].Contains(ptr));
}

// iei
