#include "Defines.h"
#include <malloc.h>

void* andi::aligned_malloc(size_t size) {
#if defined(_MSC_VER)
    return _aligned_malloc(size, Constants::Alignment);
#else
    void* ptr;
    posix_memalign(&ptr, allocAlignment, size);
    return ptr;
#endif
}

void andi::aligned_free(void* ptr) {
#if defined(_MSC_VER)
    _aligned_free(ptr);
#else
    free(ptr);
#endif
}

#if HPC_DEBUG == 1
void andi::vassert_impl(const char* expr, const char* function, const char* file, const unsigned line) {
    // iei
    static andi::mutex cerrmtx;
    andi::lock_guard lock{ cerrmtx };
    std::cerr << "Assert failed: "   << expr
              << "\n  in function: " << function
              << "\n  in file:     " << file
              << "\n  line:        " << line << "\n\n";
    int* p = nullptr;
    *p = 5;
}
#endif // HPC_DEBUG

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
    if (x & 0xFFFF'FFFFui64)
        return leastSetBit(uint32_t(x & 0xFFFF'FFFFui64));
    else if (x != 0)
        return leastSetBit(uint32_t(x >> 32)) + 32;
    else
        return 64;
}
// calculates floor(log2(x)) for every х
uint32_t fastlog2(uint32_t x) {
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
// calculates floor(log2(x)) for every x
uint32_t fastlog2(uint64_t x) {
    if (x < 0x1'0000'0000ui64)
        return fastlog2(uint32_t(x));
    else
        return 32 + fastlog2(uint32_t(x >> 32));
}

// iei
