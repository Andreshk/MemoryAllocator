#include "MemoryPool.h"

MemoryPool::MemoryPool() noexcept {
    Reset();
}

void MemoryPool::Reset() noexcept {
    poolPtr = nullptr;
    virtualZero = 0;
    for (uint32_t k = 0; k < largePoolSizeLog + 2; k++) {
        for (uint32_t i = 0; i < largePoolSizeLog + 1; i++) {
            freeBlocks[k][i].prev = nullptr;
            freeBlocks[k][i].next = nullptr;
        }
        bitvectors[k] = 0;
        leastSetBits[k] = 0;
    }
}

void MemoryPool::Initialize() noexcept {
    const size_t poolSize = size_t(1) << largePoolSizeLog;
    // Allocate the pool address space...
    poolPtr = (byte*)andi::aligned_malloc(poolSize + 32, 32);
    virtualZero = uintptr_t(poolPtr) + 32 - headerSize;
    // ...initialize the system information...
    for (uint32_t k = 0; k < largePoolSizeLog + 2; k++)
    {
        for (uint32_t i = 0; i < largePoolSizeLog + 1; i++)
        {
            freeBlocks[k][i].prev = &freeBlocks[k][i];
            freeBlocks[k][i].next = &freeBlocks[k][i];
            // no need to maintain free,k,i
        }
        bitvectors[k] = 0ui64;
        leastSetBits[k] = 64U;
    }
    // ... and add the initial Superblock
    Superblock* sblk = (Superblock*)virtualZero;
    sblk->free = 1;
    sblk->k = largePoolSizeLog + 1;
#if HPC_DEBUG == 1
    sign(sblk);
#endif
    insertFreeSuperblock(sblk);
}

void MemoryPool::Deinitialize() noexcept {
    andi::aligned_free(poolPtr);
    Reset();
}

void* MemoryPool::Allocate(size_t n) noexcept {
    if (n > allocatorMaxSize)
        return nullptr;
    mtx.lock();
    void* ptr = allocateSuperblock(n);
    mtx.unlock();
    return ptr;
}

void MemoryPool::Deallocate(void* ptr) noexcept(isRelease) {
    mtx.lock();
#if HPC_DEBUG == 1
    if (!isValidSignature(fromUserAddress(ptr))) {
        mtx.unlock();
        throw std::runtime_error("MemoryArena: Pointer is either already freed or is not the one, returned to user!\n");
    }
#endif // HPC_DEBUG
    Superblock* sblk = fromUserAddress(ptr);
    deallocateSuperblock(sblk);
    mtx.unlock();
}

size_t MemoryPool::max_size() noexcept {
    return allocatorMaxSize;
}

bool MemoryPool::isInside(void* ptr) const noexcept {
    return ptr >= poolPtr && ptr < (poolPtr + largePoolSize + 32);
}

void MemoryPool::printCondition() const {
    std::cout << "Pool address: 0x" << std::hex << (void*)poolPtr << std::dec << "\n";
    std::cout << "Pool size:  " << largePoolSize << " bytes.\n";
    std::cout << "Free superblocks of type (k,i):\n";
    size_t counter, free_space = 0;
    for (uint32_t k = 0; k < largePoolSizeLog + 2; k++)
    for (uint32_t i = 0; i < largePoolSizeLog + 1; i++) {
        counter = 0;
        const Superblock* headPtr = &freeBlocks[k][i];
        for (const Superblock* ptr = headPtr->next; ptr != headPtr; ptr = ptr->next)
            ++counter;
        if (counter != 0)
            std::cout << " (" << k << "," << i << "): " << counter << "\n";
        free_space += counter * ((size_t(1) << k) - (size_t(1) << i));
    }
    std::cout << "Free space: " << free_space << " bytes.\n";
    std::cout << "Used space: " << largePoolSize - free_space << " bytes.\n\n";
}

#if HPC_DEBUG == 1
void MemoryPool::sign(Superblock* sblk) noexcept {
    sblk->signature = getSignature(sblk);
}

uint32_t MemoryPool::getSignature(Superblock* sblk) noexcept {
    return (~sblk->blueprint) ^ uint32_t(uintptr_t(sblk) >> 8);
}

bool MemoryPool::isValidSignature(Superblock* sblk) noexcept {
    /* The probability of a false positive (a random address containing
     * a valid signature) is 1/65536 * 28/65536 * 1/2^32, or approximately
     * 1 in 600,000,000,000,000,000 (!). What's more, since the address
     * itself is used in generating the signature, at every following run
     * the probability is multiplied by this number: 1 in 10^35, 1 на 10^53,
     * ...in general, practically zero just after the first run */ 
    return (sblk->free==0
         && sblk->k > minBlockSizeLog
         && sblk->k <= largePoolSizeLog + 1
         && sblk->signature == getSignature(sblk));
}
#endif // HPC_DEBUG

void* MemoryPool::allocateSuperblock(size_t n) noexcept {
    const uint32_t j = calculateJ(n);
    Superblock* sblk = findFreeSuperblock(j);
    if (sblk == nullptr)
        return nullptr;

    // Remove this super block, we'll add the Superblocks it decomposes to later
    removeFreeSuperblock(sblk);
    const uint32_t old_k = sblk->k;
    const uint32_t old_i = calculateI(sblk);

    // In case splitting the block is needed
    if (old_i > j) {
        // split into three smaller Superblocks
        sblk->free = 0;
        sblk->k = j + 1;

        // update the system info about their existence
        Superblock* block1 = (Superblock*)(uintptr_t(sblk) + (uintptr_t(1) << j));
        block1->free = 1;
        block1->k = old_i;
#if HPC_DEBUG == 1
        sign(block1);
#endif
        insertFreeSuperblock(block1);

        if (old_k != old_i + 1) {
            Superblock* block2 = (Superblock*)(uintptr_t(sblk) + (uintptr_t(1) << old_i));
            block2->free = 1;
            block2->k = old_k;
#if HPC_DEBUG == 1
            sign(block2);
#endif
            insertFreeSuperblock(block2);
        }

        // In that case, we can simply return sblk + the offset;
#if HPC_DEBUG == 1
        sign(sblk);
#endif
        return toUserAddress(sblk);
    }

    // Calculates where in the Superblock the user address should point to
    Superblock* addr = (Superblock*)(uintptr_t(sblk) + (uintptr_t(1) << j) - (uintptr_t(1) << old_i));
    // Mark as a used block & save its k,i
    addr->free = 0;
    addr->k = j + 1;

    // A "left" Superblock may not exist (!)
    if (j > old_i) {
        // This means the Superblock found remains free, but we only update its k
        sblk->k = j;
#if HPC_DEBUG == 1
        sign(sblk);
#endif
        // ъпдейтваме системната информация
        insertFreeSuperblock(sblk);
    }
    // A "right" super block may not be needed, too
    if (j < old_k - 1) {
        // Again, mark as free and update its k,i
        Superblock* rblock = (Superblock*)(uintptr_t(addr) + (uintptr_t(1) << j));
        rblock->free = 1;
        rblock->k = old_k;
#if HPC_DEBUG == 1
        sign(rblock);
#endif
        // update the system info
        insertFreeSuperblock(rblock);
    }

    // return the address + offset
#if HPC_DEBUG == 1
    sign(addr);
#endif
    return toUserAddress(addr);
}

void MemoryPool::deallocateSuperblock(Superblock* sblk) noexcept {
    // Marks the Superblock as free and begins to
    // merge it upwards, recursively
    sblk->free = 1;
    recursiveMerge(sblk);
}

void MemoryPool::insertFreeSuperblock(Superblock* sblk) noexcept {
    // Add this Superblock to the corresponding list in the table
    const uint32_t k = sblk->k;
    const uint32_t i = calculateI(sblk);
    sblk->next = freeBlocks[k][i].next;
    freeBlocks[k][i].next = sblk;
    sblk->prev = &freeBlocks[k][i]; // == ptr->next->prev
    sblk->next->prev = sblk;
    // Update the bitvector, that a free Superblock of this size now is sure to exist
    bitvectors[k] |= (1ui64 << i);
    leastSetBits[k] = leastSetBit(bitvectors[k]);	
}

void MemoryPool::removeFreeSuperblock(Superblock* sblk) noexcept {
    // Remove the Superblock from the system info
    sblk->prev->next = sblk->next;
    sblk->next->prev = sblk->prev;
    //ptr->next = ptr->prev = nullptr;
    const uint32_t k = sblk->k;
    const uint32_t i = calculateI(sblk);
    // If there are no more Superblocks of size (k,i),
    // indicated by the list having only one element,
    // we free the i-th bit of the k-th bitvector
    if (freeBlocks[k][i].next == &freeBlocks[k][i]) {
        bitvectors[k] &= ~(1ui64 << i);
        leastSetBits[k] = leastSetBit(bitvectors[k]);
    }
}

Superblock* MemoryPool::findFreeSuperblock(uint32_t j) const noexcept {
    uint32_t min_i = 64, min_k = 0;
    for (uint32_t k = j + 1; k < largePoolSizeLog + 2; k++)
        if (leastSetBits[k] < min_i) {
            min_i = leastSetBits[k];
            min_k = k;
    }
    if (min_i == 64)
        return nullptr;
    return freeBlocks[min_k][min_i].next;
}

Superblock* MemoryPool::findBuddySuperblock(Superblock* sblk) const noexcept {
    // Finding a Superblock's buddy is as simple as flipping the i+1-st bit of its virtual address
    return fromVirtualOffset(toVirtualOffset(sblk) ^ (uintptr_t(1) << calculateI(sblk)));
}

void MemoryPool::recursiveMerge(Superblock* sblk) noexcept {
    // Superblocks are merged only if these three conditions hold:
    // - there is something left to merge (i.e. the pool isn't completely empty)
    // - the current Superblock's buddy is free
    // - the buddy has the appropriate size of 2^k-2^i for some k,
    //   where 2^i is the size of the current block
    // Otherwise, the block is simply inserted to its corresponding
    // list, as a normal block of size 2^j for some j
    Superblock* buddy = findBuddySuperblock(sblk);
    if ((uintptr_t(sblk) == virtualZero && sblk->k == largePoolSizeLog + 1) ||
        buddy->free == 0 || calculateI(sblk) != calculateI(buddy)) {
#if HPC_DEBUG == 1
        sign(sblk);
#endif
        insertFreeSuperblock(sblk);
        return;
    }
    // There will be a merge, so we remove the buddy from the system info
    removeFreeSuperblock(buddy);
    const uint32_t buddy_k = buddy->k;	// старото k
    // Unite the buddies in a block of size 2^k (again, represented as 2^(k+1) - 2^k)
    if (buddy < sblk)
        sblk = buddy;
    sblk->k = buddy_k + 1;
    // Merge upwards until possible.
    recursiveMerge(sblk); // Tail recursion optimization should probably take care of this call.
}

void* MemoryPool::toUserAddress(Superblock* sblk) noexcept {
    return (void*)(uintptr_t(sblk) + headerSize);
}

Superblock* MemoryPool::fromUserAddress(void* ptr) noexcept {
    return (Superblock*)(uintptr_t(ptr) - headerSize);
}

uintptr_t MemoryPool::toVirtualOffset(Superblock* sblk) const noexcept {
    return uintptr_t(sblk) - virtualZero;
}

Superblock* MemoryPool::fromVirtualOffset(uintptr_t offset) const noexcept {
    return (Superblock*)(virtualZero + offset);
}

uint32_t MemoryPool::calculateI(Superblock* sblk) const noexcept {
    return min(leastSetBit(toVirtualOffset(sblk)), sblk->k - 1);
}

uint32_t MemoryPool::calculateJ(size_t n) noexcept {
    // this is a harmless cast iff allocatorMaxSize can fit in 32 bits
    return max(fastlog2(uint32_t(n + headerSize - 1)) + 1, minBlockSizeLog);
}

uint32_t min(uint32_t a, uint32_t b) { return (a < b) ? a : b; }
uint32_t max(uint32_t a, uint32_t b) { return (a > b) ? a : b; }
uint64_t max(uint64_t a, uint64_t b) { return (a > b) ? a : b; }

uint32_t leastSetBit(uint32_t x) {
    static const uint32_t DeBruijnLeastSetBit[32] = {
        0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4, 8,
        31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6, 11, 5, 10, 9
    };
    return DeBruijnLeastSetBit[((x & (~x + 1U)) * 0x077CB531U) >> 27];
}

uint32_t leastSetBit(uint64_t x) {
    if (x & 0xFFFFFFFFui64)
        return leastSetBit(uint32_t(x & 0xFFFFFFFFui64));
    else if (x != 0)
        return leastSetBit(uint32_t(x >> 32)) + 32;
    else
        return 64;
}

uint32_t fastlog2(uint32_t x) { // calculates floor(log2(x)) for every х
    static const uint32_t DeBruijnLog2Inexact[32] = {
        0, 9, 1, 10, 13, 21, 2, 29, 11, 14, 16, 18, 22, 25, 3, 30,
        8, 12, 20, 28, 15, 17, 24, 7, 19, 27, 23, 6, 26, 5, 4, 31
    };

    x |= x >> 1; // first round down to one less than a power of 2 
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;

    return DeBruijnLog2Inexact[(x * 0x07C4ACDDU) >> 27];
}

uint32_t fastlog2(uint64_t x) { // calculates floor(log2(x)) for every x
    if (x < 0x100000000ui64)
        return fastlog2(uint32_t(x));
    else
        return 32 + fastlog2(uint32_t(x >> 32));
}

// iei
