#include "Commons.h"

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
void andi::vassert_impl(const char* expr, const char* file, const unsigned line) {
    // iei
    int* p = nullptr;
    *p = 5;
}
#endif // HPC_DEBUG

// iei
