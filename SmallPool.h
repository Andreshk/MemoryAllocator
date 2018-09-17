#pragma once
#include "Commons.h"

template<size_t N, size_t Count>
class SmallPool {
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

    SmallPool(); // no destructor, we rely on Deinitialize
    void Reset();
    void Initialize();
    void Deinitialize();

    void* Allocate();
    void Deallocate(void*);
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
    SmallPool(const SmallPool&) = delete;
    SmallPool& operator=(const SmallPool&) = delete;
    SmallPool(SmallPool&&) = delete;
    SmallPool& operator=(SmallPool&&) = delete;
};

template<size_t N, size_t Count>
SmallPool<N, Count>::SmallPool() {
    Reset();
}

template<size_t N, size_t Count>
void SmallPool<N, Count>::Reset() {
    blocksPtr = nullptr;
    headIdx = Constants::InvalidIdx;
    allocatedBlocks = 0;
}

template<size_t N, size_t Count>
void SmallPool<N, Count>::Initialize() {
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
void SmallPool<N, Count>::Deinitialize() {
    andi::aligned_free(blocksPtr);
    Reset();
}

template<size_t N, size_t Count>
void* SmallPool<N, Count>::Allocate() {
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
void SmallPool<N, Count>::Deallocate(void* sblk) {
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
void SmallPool<N, Count>::printCondition() const {
    std::cout << "SmallPool<" << N << ">:\n"
        << "  pool size:  " << Count * N << " bytes (" << Count << " blocks)\n"
        << "  free space: " << (Count - allocatedBlocks)*N << " bytes (" << Count - allocatedBlocks << " blocks)\n"
        << "  used space: " << allocatedBlocks*N << " bytes (" << allocatedBlocks << " blocks)\n\n";
}

template<size_t N, size_t Count>
bool SmallPool<N, Count>::isInside(void* ptr) const {
    return ptr >= blocksPtr && ptr < (blocksPtr + Count);
}

#if HPC_DEBUG == 1
template<size_t N, size_t Count>
void SmallPool<N, Count>::signFreeBlock(Smallblock* ptr) {
    ptr->pad[0] = getSignature(ptr);
}

template<size_t N, size_t Count>
void SmallPool<N, Count>::unsignFreeBlock(Smallblock* ptr) {
    ptr->pad[0] = 0;
}

template<size_t N, size_t Count>
size_t SmallPool<N, Count>::getSignature(Smallblock* ptr) {
    return ~size_t(ptr);
}

template<size_t N, size_t Count>
bool SmallPool<N, Count>::isSigned(Smallblock* ptr) {
    // There is a 1 in 2^64 chance of a false positive,
    // decreasing exponentially every time the program is ran.
    return (ptr->pad[0] == getSignature(ptr));
}
#endif // HPC_DEBUG

// iei
