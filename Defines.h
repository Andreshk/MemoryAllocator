#pragma once
// These definitions control the allocator behaviour (see README.md)
#define HPC_DEBUG 1
#define USE_POOL_ALLOCATORS 0

#include "Utilities.h"

enum Constants : size_t {
    // Minimum alignment for allocation requests
    Alignment = 32,
    // Logarithm of the buddy allocator address space size in bytes
    K = (sizeof(void*) == 8) ? 31 : 29,
    // The buddy allocators need to have their size a power of 2:
    // 2GB in 64-bit mode, 512MB in 32-bit
    BuddyAllocatorSize = size_t(1) << K,
    // Superblock header size, in bytes
    HeaderSize = sizeof(SuperblockHeader),
    // Invalid block index for the small pools
    InvalidIdx = ~size_t(0),
    // Logarithm of the smallest allocation size, in bytes
    MinAllocationSizeLog = 5,
    // Minimum allocation size, in bytes
    MinAllocationSize = size_t(1) << MinAllocationSizeLog,
    // The upper limit for a single allocation
    MaxAllocationSize = (Constants::BuddyAllocatorSize / 4) - Constants::HeaderSize,
    // Number of blocks in the fixed-size pools:
    PoolSize0 = 1'500'000, //   32B
    PoolSize1 = 1'500'000, //   64B
    PoolSize2 =   500'000, //  128B
    PoolSize3 =   250'000, //  256B
    PoolSize4 =   200'000, //  512B
    PoolSize5 =   200'000, // 1024B
};
// Scoped enums are nice, but require overly verbose conversions to the underlying type...

// Sanity checks for global constants' validity
static_assert(Constants::HeaderSize < Constants::Alignment);
static_assert(Constants::Alignment >= sizeof(size_t)); // required for PoolAllocator::Smallblock
static_assert(Constants::Alignment % alignof(Superblock) == 0); // virtualZero should be a valid Superblock address
static_assert(Constants::K <= 63); // we want (2^largePoolSizeLog) to fit in 64 bits
static_assert(Constants::HeaderSize < Constants::MinAllocationSize); // otherwise headers overlap and mayhem ensues
static_assert(Constants::MinAllocationSizeLog >= 5
           && Constants::MinAllocationSizeLog <= Constants::K);
static_assert(Constants::MaxAllocationSize <= Constants::BuddyAllocatorSize
           && Constants::MaxAllocationSize + Constants::HeaderSize <= 0x1'0000'0000ui64);
static_assert(offsetof(Superblock, prev) == Constants::HeaderSize); // fun fact: this causes undefined behaviour
