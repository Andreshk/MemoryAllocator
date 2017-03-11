#include "Commons.h"

void* andi::aligned_malloc(size_t _Size, size_t _Alignment) noexcept
{
#if defined(_MSC_VER)
	return _aligned_malloc(_Size, _Alignment);
#else
	void* ptr;
	posix_memalign(&ptr, _Alignment, _Size);
	return ptr;
#endif
}

void andi::aligned_free(void* _Ptr) noexcept
{
#if defined(_MSC_VER)
	_aligned_free(_Ptr);
#else
	free(_Ptr);
#endif
}

void andi::mutex::lock() noexcept
{
	bool isLocked = false;
	while (!locked.compare_exchange_strong(isLocked, true))
		isLocked = false;
}

void andi::mutex::unlock() noexcept
{
	locked = false;
}

// iei
