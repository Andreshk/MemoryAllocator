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
constexpr size_t headerSize = offsetof(Superblock, prev); // fun fact: this causes undefined behaviour
// The upper limit for a single allocation
constexpr size_t allocatorMaxSize = (Constants::BuddyAllocatorSize / 4) - headerSize;

// Sanity checks for global constants' validity
static_assert(headerSize < Constants::Alignment);
static_assert(Constants::Alignment % alignof(Superblock) == 0); // virtualZero should be a valid Superblock address
static_assert(Constants::K <= 63); // we want (2^largePoolSizeLog) to fit in 64 bits
static_assert(headerSize < Constants::MinAllocationSize); // otherwise headers overlap and mayhem ensues
static_assert(Constants::MinAllocationSizeLog >= 5
           && Constants::MinAllocationSizeLog <= Constants::K);
static_assert(allocatorMaxSize < Constants::BuddyAllocatorSize
           && allocatorMaxSize + headerSize - 1 < 0x1'0000'0000ui64);

/*
 - The memory returned to the user is allocated from a large address space (pool)
 with a power of two size (f.e. 4GB). This address space is allocated from the
 system once, on initialization and never changes before deinitialization.
 - The pool's state is cotrolled by a table of Superblocks, in which at
 position (k,i) we keep a list of Superblocks of size 2^k-2^i bytes,
 containing all other free Superblocks of the same size.
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
    Superblock freeBlocks[Constants::K + 2][Constants::K + 1];
    uint64_t bitvectors[Constants::K + 2];
    uint32_t leastSetBits[Constants::K + 2];
    byte* poolPtr;
    uintptr_t virtualZero;
    andi::mutex mtx;

    MemoryPool(); // no destructor, we rely on Deinitialize
    void Reset();
    void Initialize();
    void Deinitialize();

    void* Allocate(size_t);
    void Deallocate(void*);
    static size_t max_size();
    bool isInside(void*) const;
    void printCondition() const;
#if HPC_DEBUG == 1
    static void sign(Superblock*);
    static uint32_t getSignature(Superblock*);
    static bool isValidSignature(Superblock*);
#endif // HPC_DEBUG

    void* allocateSuperblock(size_t);
    void deallocateSuperblock(Superblock*);
    void insertFreeSuperblock(Superblock*);
    void removeFreeSuperblock(Superblock*);
    Superblock* findFreeSuperblock(uint32_t) const;
    Superblock* findBuddySuperblock(Superblock*) const;
    void recursiveMerge(Superblock*);
    static void* toUserAddress(Superblock*);
    static Superblock* fromUserAddress(void*);
    uintptr_t toVirtualOffset(Superblock*) const;
    Superblock* fromVirtualOffset(uintptr_t) const;
    uint32_t calculateI(Superblock*) const;
    static uint32_t calculateJ(size_t);

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
