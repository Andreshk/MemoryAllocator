#pragma once
#include "Defines.h"
#include "Utilities.h"

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
class BuddyAllocator {
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

    BuddyAllocator(); // no destructor, we rely on Deinitialize
    void Reset();
    void Initialize();
    void Deinitialize();

    void* Allocate(size_t);
    void Deallocate(void*);
    std::pair<void*, size_t> AllocateUseful(size_t);
    static size_t MaxSize();
    bool Contains(void*) const;
    void PrintCondition() const;
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
    BuddyAllocator(const BuddyAllocator&) = delete;
    BuddyAllocator& operator=(const BuddyAllocator&) = delete;
    BuddyAllocator(BuddyAllocator&&) = delete;
    BuddyAllocator& operator=(BuddyAllocator&&) = delete;
};

// iei
