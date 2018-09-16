#pragma once
#include "Commons.h"

template<size_t N, size_t Count>
class SmallPool {
    // forward declaration...
    friend class MemoryArena;

    static_assert(N >= 32 && !(N&(N - 1)),
        "N has to be a power of two, no less than 32!");
    struct Smallblock {
        size_t next;
        size_t pad[N / sizeof(size_t) - 1];
    };

    Smallblock* blocksPtr;
    size_t headIdx;
    size_t allocatedBlocks;
    andi::mutex mtx;

    SmallPool() noexcept; // no destructor, we rely on Deinitialize
    void Reset() noexcept;
    void Initialize() noexcept;
    void Deinitialize() noexcept;

    void* Allocate() noexcept;
    void Deallocate(void*) noexcept(isRelease);
    void printCondition() const;
    bool isInside(void*) const noexcept;
#if HPC_DEBUG == 1
    // Here the signatures work in the other way - only the free blocks are signed
    static void signFreeBlock(Smallblock*) noexcept;
    static void unsignFreeBlock(Smallblock*) noexcept;
    static size_t getSignature(Smallblock*) noexcept;
    static bool isSigned(Smallblock*) noexcept;
#endif // HPC_DEBUG

public:
    // moving or copying of pools is forbidden
    SmallPool(const SmallPool&) = delete;
    SmallPool& operator=(const SmallPool&) = delete;
    SmallPool(SmallPool&&) = delete;
    SmallPool& operator=(SmallPool&&) = delete;
};

template<size_t N, size_t Count>
SmallPool<N, Count>::SmallPool() noexcept {
    Reset();
}

template<size_t N, size_t Count>
void SmallPool<N, Count>::Reset() noexcept {
    blocksPtr = nullptr;
    headIdx = invalidIdx;
    allocatedBlocks = 0;
}

template<size_t N, size_t Count>
void SmallPool<N, Count>::Initialize() noexcept {
    blocksPtr = (Smallblock*)andi::aligned_malloc(N*Count, 32);
    for (size_t i = 0; i < Count; i++) {
        blocksPtr[i].next = i + 1;
#if HPC_DEBUG == 1
        signFreeBlock(blocksPtr + i);
#endif // HPC_DEBUG
    }
    blocksPtr[Count - 1].next = invalidIdx;
    headIdx = 0;
    allocatedBlocks = 0;
}

template<size_t N, size_t Count>
void SmallPool<N, Count>::Deinitialize() noexcept {
    andi::aligned_free(blocksPtr);
    Reset();
}

template<size_t N, size_t Count>
void* SmallPool<N, Count>::Allocate() noexcept {
    mtx.lock();
    if (headIdx == invalidIdx) {
        mtx.unlock();
        return nullptr;
    }
    size_t free = headIdx;
    headIdx = blocksPtr[free].next;
    ++allocatedBlocks;
#if HPC_DEBUG == 1
    unsignFreeBlock(blocksPtr + free);
#endif // HPC_DEBUG
    mtx.unlock();
    return blocksPtr + free;
}

template<size_t N, size_t Count>
void SmallPool<N, Count>::Deallocate(void* sblk) noexcept(isRelease) {
    mtx.lock();
    size_t idx = (Smallblock*)sblk - blocksPtr;
#if HPC_DEBUG == 1
    if (uintptr_t(sblk) % 32 != 0) {
        mtx.unlock();
        throw std::runtime_error("MemoryArena: Attempting to free a non-aligned pointer!");
    } else if (isSigned((Smallblock*)sblk)) {
        mtx.unlock();
        throw std::runtime_error("MemoryArena: attempting to free memory that has already been freed!\n");
    }
    signFreeBlock((Smallblock*)(blocksPtr + idx));
#endif // HPC_DEBUG
    --allocatedBlocks;
    blocksPtr[idx].next = headIdx;
    headIdx = idx;
    mtx.unlock();
}

template<size_t N, size_t Count>
void SmallPool<N, Count>::printCondition() const {
    std::cout << "SmallPool<" << N << ">:\n"
        << "  pool size:  " << Count * N << " bytes (" << Count << " blocks)\n"
        << "  free space: " << (Count - allocatedBlocks)*N << " bytes (" << Count - allocatedBlocks << " blocks)\n"
        << "  used space: " << allocatedBlocks*N << " bytes (" << allocatedBlocks << " blocks)\n\n";
}

template<size_t N, size_t Count>
bool SmallPool<N, Count>::isInside(void* ptr) const noexcept {
    return ptr >= blocksPtr && ptr < (blocksPtr + Count);
}

#if HPC_DEBUG == 1
template<size_t N, size_t Count>
void SmallPool<N, Count>::signFreeBlock(Smallblock* ptr) noexcept {
    ptr->pad[0] = getSignature(ptr);
}

template<size_t N, size_t Count>
void SmallPool<N, Count>::unsignFreeBlock(Smallblock* ptr) noexcept {
    ptr->pad[0] = 0;
}

template<size_t N, size_t Count>
size_t SmallPool<N, Count>::getSignature(Smallblock* ptr) noexcept {
    return ~size_t(ptr);
}

template<size_t N, size_t Count>
bool SmallPool<N, Count>::isSigned(Smallblock* ptr) noexcept {
    // There is a 1 in 2^64 chance of a false positive,
    // decreasing exponentially every time the program is ran.
    return (ptr->pad[0] == getSignature(ptr));
}
#endif // HPC_DEBUG

// iei
