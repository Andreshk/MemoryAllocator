#pragma once
#include <malloc.h>
#include <stdlib.h>
#include <atomic>
#include <iostream>
#include <exception>

// С тези дефиниции се управлява цялостното поведение на алокатора (вж. readme.txt)
#define HPC_DEBUG 0
#define USE_SMALL_POOLS 1

#if USE_SMALL_POOLS == 1
// С тези константи се управлява колко да са големи pool-овете с фиксиран размер.
// брой блокове с размер:     <=32B   64B     128B   256B   512B   1024B
constexpr size_t sizes[6] = { 1500000,1500000,500000,250000,200000,200000 };
constexpr size_t invalidIdx = ~size_t(0);
#endif // USE_SMALL_POOLS

// За големите pool-ове, които ще бъдат управлявани с buddy системата
// за алокация - техният размер задължително трябва да е степен на двойката!
#ifdef NDEBUG
constexpr uint32_t largePoolSizeLog = (sizeof(void*) == 8) ? 32 : 29;	// логаритъм от размера на адресното пространство в байтове
#else
constexpr uint32_t largePoolSizeLog = 29;
#endif
constexpr size_t largePoolSize = size_t(1) << largePoolSizeLog;			// 4GB в 64-битов режим, 512MB в 32-битов или в debug режим на компилатора

// Функции, декларирани с noexcept(isRelease) биха могли да хвърлят
// изключения само при включен HPC_DEBUG, а в противен случай са строго noexcept.
constexpr bool isRelease = (HPC_DEBUG == 0);

namespace andi
{
	void* aligned_malloc(size_t, size_t) noexcept;
	void aligned_free(void*) noexcept;

	// Малък mutex, тип busy-waiting - заменя цената на context switching
	// с цената на това нишката да остане активна, надявайки се това чакане
	// да не продължи дълго (тогава отново общият processing time нараства).
	class mutex
	{
		std::atomic<bool> locked;
	public:
		void lock() noexcept;
		void unlock() noexcept;
	};
}
