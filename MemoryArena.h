#pragma once
#include "SmallPool.h"
#include "MemoryPool.h"

// Декларираме класа-алокатор, който по-късно ще може да достъпва методите на арената.
// Естествено, памет може да се заделя и освобождава с MemoryArena::Allocate и MemoryArena::Deallocate
namespace andi { template<class> class allocator; }

// Класът MemoryArena e singleton(!) и всичките операции с памет минават през него.
// Разполага с няколко pool-а с памет, до чиито методи единствено той има достъп.
class MemoryArena
{
	template<class> friend class andi::allocator;

#if USE_SMALL_POOLS == 1
	SmallPool<32> tp0;
	SmallPool<64> tp1;
	SmallPool<128> tp2;
	SmallPool<256> tp3;
	SmallPool<512> tp4;
	SmallPool<1024> tp5;
#endif // USE_SMALL_POOLS

	std::atomic_uint32_t toggle;
	MemoryPool largePool[2];
	andi::mutex initializationmtx;
	bool _isInitialized;

	// look-up "static initialization fiasco"
	static MemoryArena arena;

	MemoryArena();
	static size_t max_size() noexcept;
	static void LockAll() noexcept;
	static void UnlockAll() noexcept;
	static bool isInside(void*) noexcept;

public:
	// не е позволено копиране/преместване на арени
	MemoryArena(const MemoryArena&) = delete;
	MemoryArena& operator=(const MemoryArena&) = delete;
	MemoryArena(MemoryArena&&) = delete;
	MemoryArena& operator=(MemoryArena&&) = delete;

	static bool Initialize() noexcept(isRelease);
	static bool Deinitialize() noexcept(isRelease);
    static bool isInitialized() noexcept;
	static void* Allocate(size_t) noexcept(isRelease);
	static void Deallocate(void*) noexcept(isRelease);
	// невероятно полезна функция за отпечатване на
	// състоянието на адресното пространство на бъди алокатора
	static void printCondition();
};
