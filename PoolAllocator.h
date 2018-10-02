#pragma once
#include "Defines.h"
#include "Utilities.h"

template<size_t N, size_t Count>
class PoolAllocator {
    // forward declaration...
    friend class MemoryArena;

    static_assert(N >= Constants::Alignment && !(N&(N - 1)),
        "N has to be a power of two, no less than the alignment requirement!");
    struct Smallblock {
        size_t next;
        size_t pad[N / sizeof(size_t) - 1];
    };

    Smallblock* blocksPtr;
    size_t headIdx;
    size_t allocatedBlocks;
    andi::mutex mtx;

    PoolAllocator(); // no destructor, we rely on Deinitialize
    void Reset();
    void Initialize();
    void Deinitialize();

    void* Allocate();
    void Deallocate(void*);
    std::pair<void*, size_t> AllocateUseful();
    void printCondition() const;
    bool isInside(void*) const;
#if HPC_DEBUG == 1
    // Here the signatures work in the other way - only the free blocks are signed
    static void signFreeBlock(Smallblock*);
    static void unsignFreeBlock(Smallblock*);
    static size_t getSignature(Smallblock*);
    static bool isSigned(Smallblock*);
#endif // HPC_DEBUG

public:
    // moving or copying of pools is forbidden
    PoolAllocator(const PoolAllocator&) = delete;
    PoolAllocator& operator=(const PoolAllocator&) = delete;
    PoolAllocator(PoolAllocator&&) = delete;
    PoolAllocator& operator=(PoolAllocator&&) = delete;
};

template<size_t N, size_t Count>
PoolAllocator<N, Count>::PoolAllocator() {
    Reset();
}

template<size_t N, size_t Count>
void PoolAllocator<N, Count>::Reset() {
    blocksPtr = nullptr;
    headIdx = Constants::InvalidIdx;
    allocatedBlocks = 0;
}

template<size_t N, size_t Count>
void PoolAllocator<N, Count>::Initialize() {
    andi::lock_guard lock{ mtx };
    blocksPtr = (Smallblock*)andi::aligned_malloc(N*Count);
    for (size_t i = 0; i < Count; i++) {
        blocksPtr[i].next = i + 1;
#if HPC_DEBUG == 1
        signFreeBlock(blocksPtr + i);
#endif // HPC_DEBUG
    }
    blocksPtr[Count - 1].next = Constants::InvalidIdx;
    headIdx = 0;
    allocatedBlocks = 0;
}

template<size_t N, size_t Count>
void PoolAllocator<N, Count>::Deinitialize() {
    andi::lock_guard lock{ mtx };
    andi::aligned_free(blocksPtr);
    Reset();
}

template<size_t N, size_t Count>
void* PoolAllocator<N, Count>::Allocate() {
    andi::lock_guard lock{ mtx };
    if (headIdx == Constants::InvalidIdx)
        return nullptr;

    size_t free = headIdx;
    headIdx = blocksPtr[free].next;
    ++allocatedBlocks;
#if HPC_DEBUG == 1
    unsignFreeBlock(blocksPtr + free);
#endif // HPC_DEBUG
    return blocksPtr + free;
}

template<size_t N, size_t Count>
void PoolAllocator<N, Count>::Deallocate(void* sblk) {
    andi::lock_guard lock{ mtx };
    size_t idx = (Smallblock*)sblk - blocksPtr;
    vassert(uintptr_t(sblk) % Constants::Alignment == 0
        && "MemoryArena: Attempting to free a non-aligned pointer!");
    vassert(isSigned((Smallblock*)sblk)
        && "MemoryArena: attempting to free memory that has already been freed!\n");
#if HPC_DEBUG == 1
    signFreeBlock((Smallblock*)(blocksPtr + idx));
#endif // HPC_DEBUG
    --allocatedBlocks;
    blocksPtr[idx].next = headIdx;
    headIdx = idx;
}

template<size_t N, size_t Count>
std::pair<void*, size_t> PoolAllocator<N, Count>::AllocateUseful() {
    return { Allocate(), N };
}

template<size_t N, size_t Count>
void PoolAllocator<N, Count>::printCondition() const {
    std::cout << "PoolAllocator<" << N << "," << Count << ">:\n"
        << "  pool size:  " << Count * N << " bytes (" << Count << " blocks)\n"
        << "  free space: " << (Count - allocatedBlocks)*N << " bytes (" << Count - allocatedBlocks << " blocks)\n"
        << "  used space: " << allocatedBlocks*N << " bytes (" << allocatedBlocks << " blocks)\n\n";
}

template<size_t N, size_t Count>
bool PoolAllocator<N, Count>::isInside(void* ptr) const {
    return ptr >= blocksPtr && ptr < (blocksPtr + N*Count);
}

#if HPC_DEBUG == 1
template<size_t N, size_t Count>
void PoolAllocator<N, Count>::signFreeBlock(Smallblock* ptr) {
    ptr->pad[0] = getSignature(ptr);
}

template<size_t N, size_t Count>
void PoolAllocator<N, Count>::unsignFreeBlock(Smallblock* ptr) {
    ptr->pad[0] = 0;
}

template<size_t N, size_t Count>
size_t PoolAllocator<N, Count>::getSignature(Smallblock* ptr) {
    return ~size_t(ptr);
}

template<size_t N, size_t Count>
bool PoolAllocator<N, Count>::isSigned(Smallblock* ptr) {
    // There is a 1 in 2^64 chance of a false positive,
    // decreasing exponentially every time the program is ran.
    return (ptr->pad[0] == getSignature(ptr));
}
#endif // HPC_DEBUG

// iei
