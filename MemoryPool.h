#pragma once
#include "Commons.h"

// Contains the necessary information for managing the free "superblocks"
struct Superblock {
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
    // All previous member data form the so-called header.
    // Fun fact: in theory this header can be reduced to 7 bits (!) -> O(lglgn)
    Superblock* prev;
    Superblock* next;
};

// Superblock header size, in bytes
constexpr size_t   headerSize = offsetof(Superblock, prev);
// Logarithm of the smallest block size, which can be allocated
constexpr uint32_t minBlockSizeLog = 5;
// The upper limit for a single allocation
constexpr size_t   allocatorMaxSize = (largePoolSize / 4) - headerSize;

// Sanity check for global constants' validity
static_assert(headerSize < 32 && largePoolSizeLog <= 63 && headerSize < (size_t(1) << minBlockSizeLog) &&
              minBlockSizeLog >= 5 && minBlockSizeLog <= largePoolSizeLog && allocatorMaxSize < largePoolSize &&
              allocatorMaxSize + headerSize - 1 < 0x100000000ui64, "Invalid global variable values!");

/*
 - The memory returned to the user is allocated from a large address space (pool)
 with a power of two size (f.e. 4GB). This address space is allocated from the
 system once, on initialization and never changes before deinitialization.
 - The pool's state is cotrolled by a table of Superblocks, in which at
 position (k,i) we keep a Superblock of size 2^k-2^i bytes, pointing to
 the other free Superblocks of the same size.
 - The free Superblocks of a given size are linked in a doubly-connected
 cyclic (!) linked list, since merging requires removing of a block at a
 random position in its list. Being cyclic and always non-empty alleviates
 some nullptr checks, speeding up adding and removing blocks. The largest
 Superblock captures the entire address space and has size 2^largePoolSizeLog,
 internally represented as 2^(largePoolSizeLog+1) - 2^largePoolSizeLog
 - For each k there is a bitvector, where the i-th bit is toggled iff
 there exists a free Superblock of size 2^k-2^i. They are used to select
 the most proper Superblock size, for a given allocation request.
 - Finally, for each bitvector we keep the lowest toggled bit. This is
 used during searching for a suitable block of memory
*/
class MemoryPool {
    // forward declaration...
    friend class MemoryArena;
    using byte = uint8_t;

private:
    byte* poolPtr;
    uintptr_t virtualZero;
    Superblock freeBlocks[largePoolSizeLog + 2][largePoolSizeLog + 1];
    uint64_t bitvectors[largePoolSizeLog + 2];
    uint32_t leastSetBits[largePoolSizeLog + 2];
    andi::mutex mtx;

    MemoryPool() noexcept; // no destructor, we rely on Deinitialize
    void Reset() noexcept;
    void Initialize() noexcept;
    void Deinitialize() noexcept;

    void* Allocate(size_t) noexcept;
    void Deallocate(void*) noexcept(isRelease);
    static size_t max_size() noexcept;
    bool isInside(void*) const noexcept;
    void printCondition() const;
#if HPC_DEBUG == 1
    static void sign(Superblock*) noexcept;
    static uint32_t getSignature(Superblock*) noexcept;
    static bool isValidSignature(Superblock*) noexcept;
#endif // HPC_DEBUG

    void* allocateSuperblock(size_t) noexcept;
    void deallocateSuperblock(void*) noexcept;
    void insertFreeSuperblock(Superblock*) noexcept;
    void removeFreeSuperblock(Superblock*) noexcept;
    Superblock* findFreeSuperblock(uint32_t) const noexcept;
    Superblock* findBuddySuperblock(Superblock*) const noexcept;
    void recursiveMerge(Superblock*) noexcept;
    static void* toUserAddress(Superblock*) noexcept;
    static Superblock* fromUserAddress(void*) noexcept;
    uintptr_t toVirtualOffset(Superblock*) const noexcept;
    Superblock* fromVirtualOffset(uintptr_t) const noexcept;
    uint32_t calculateI(Superblock*) const noexcept;
    static uint32_t calculateJ(size_t) noexcept;

public:
    // moving or copying of pools is forbidden
    MemoryPool(const MemoryPool&) = delete;
    MemoryPool& operator=(const MemoryPool&) = delete;
    MemoryPool(MemoryPool&&) = delete;
    MemoryPool& operator=(MemoryPool&&) = delete;
};

// Helper math functions
uint32_t min(uint32_t, uint32_t);
uint32_t max(uint32_t, uint32_t);
uint64_t max(uint64_t, uint64_t);
uint32_t leastSetBit(uint32_t);
uint32_t leastSetBit(uint64_t);
uint32_t fastlog2(uint32_t);
uint32_t fastlog2(uint64_t);

// iei
