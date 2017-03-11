﻿#include "MemoryArena.h"

MemoryArena MemoryArena::arena{};

MemoryArena::MemoryArena() : _isInitialized(false) {}

bool MemoryArena::Initialize() noexcept(isRelease)
{
	arena.initializationmtx.lock();
	if (arena._isInitialized)
	{
		arena.initializationmtx.unlock();
#if HPC_DEBUG == 1
		throw std::runtime_error("MemoryArena has already been initialized!");
#endif // HPC_DEBUG
		return false;
	}
	LockAll();

#if USE_SMALL_POOLS == 1
	arena.tp0.Initialize(sizes[0]);
	arena.tp1.Initialize(sizes[1]);
	arena.tp2.Initialize(sizes[2]);
	arena.tp3.Initialize(sizes[3]);
	arena.tp4.Initialize(sizes[4]);
	arena.tp5.Initialize(sizes[5]);
#endif // USE_SMALL_POOLS
    
	arena.largePool[0].Initialize();
	arena.largePool[1].Initialize();
	arena._isInitialized = true;
	UnlockAll();
	arena.initializationmtx.unlock();
	return true;
}

bool MemoryArena::Deinitialize() noexcept(isRelease)
{
	arena.initializationmtx.lock();
	if (!arena._isInitialized)
	{
		arena.initializationmtx.unlock();
#if HPC_DEBUG == 1
		throw std::runtime_error("MemoryArena has already been deinitialized!");
#endif // HPC_DEBUG
		return false;
	}
	LockAll();

#if USE_SMALL_POOLS == 1
	arena.tp0.Deinitialize();
	arena.tp1.Deinitialize();
	arena.tp2.Deinitialize();
	arena.tp3.Deinitialize();
	arena.tp4.Deinitialize();
	arena.tp5.Deinitialize();
#endif // USE_SMALL_POOLS
    
	arena.largePool[0].Deinitialize();
	arena.largePool[1].Deinitialize();
	arena._isInitialized = false;
	UnlockAll();
	arena.initializationmtx.unlock();
	return true;
}

bool MemoryArena::isInitialized() noexcept
{
    return arena._isInitialized;
}

void* MemoryArena::Allocate(size_t n) noexcept(isRelease)
{
	if (n == 0)
		return nullptr;
	void* ptr = nullptr;

#if HPC_DEBUG == 1
	if (!arena._isInitialized)
		throw std::runtime_error("MemoryArena must be initialized before allocation!\n");
#endif // HPC_DEBUG

#if USE_SMALL_POOLS == 0
	const uint32_t idx = arena.toggle.fetch_add(1) & 1;
	ptr = arena.largePool[idx].Allocate(n);
#else
	if (n <= 32)
		ptr = arena.tp0.Allocate();
	else if (n <= 64)
		ptr = arena.tp1.Allocate();
	else if (n <= 128)
		ptr = arena.tp2.Allocate();
	else if (n <= 256)
		ptr = arena.tp3.Allocate();
	else if (n <= 512)
		ptr = arena.tp4.Allocate();
	else if (n <= 1024)
		ptr = arena.tp5.Allocate();
	if (ptr == nullptr) {
		const uint32_t idx = arena.toggle.fetch_add(1) & 1;
		ptr = arena.largePool[idx].Allocate(n);
	}
#endif // USE_SMALL_POOLS
    return ptr;
}

void MemoryArena::Deallocate(void* _Ptr) noexcept(isRelease)
{
	if (!_Ptr)
		return;

#if HPC_DEBUG == 1
	if (!arena._isInitialized)
		throw std::runtime_error("MemoryArena must be initialized before deallocation!\n");
	if (!arena.isInside(_Ptr))
		throw std::runtime_error("MemoryArena: pointer is outside of the address space!\n");
#endif // HPC_DEBUG

#if USE_SMALL_POOLS == 0
	if (arena.largePool[0].isInside(_Ptr))
		arena.largePool[0].Deallocate(_Ptr);
	else
		arena.largePool[1].Deallocate(_Ptr);
#else
	if (arena.tp0.isInside(_Ptr))
		arena.tp0.Deallocate(_Ptr);
	else if (arena.tp1.isInside(_Ptr))
		arena.tp1.Deallocate(_Ptr);
	else if (arena.tp2.isInside(_Ptr))
		arena.tp2.Deallocate(_Ptr);
	else if (arena.tp3.isInside(_Ptr))
		arena.tp3.Deallocate(_Ptr);
	else if (arena.tp4.isInside(_Ptr))
		arena.tp4.Deallocate(_Ptr);
	else if (arena.tp5.isInside(_Ptr))
		arena.tp5.Deallocate(_Ptr);
	else if (arena.largePool[0].isInside(_Ptr))
		arena.largePool[0].Deallocate(_Ptr);
	else
		arena.largePool[1].Deallocate(_Ptr);
#endif // USE_SMALL_POOLS && USE_TEMPL_POOLS
}

void MemoryArena::printCondition()
{
#if USE_SMALL_POOLS == 1
    arena.tp0.printCondition();
    arena.tp1.printCondition();
    arena.tp2.printCondition();
    arena.tp3.printCondition();
    arena.tp4.printCondition();
    arena.tp5.printCondition();
#endif // USE_SMALL_POOLS

	arena.largePool[0].printCondition();
	arena.largePool[1].printCondition();
}

size_t MemoryArena::max_size() noexcept
{
	return MemoryPool::max_size();
}

void MemoryArena::LockAll() noexcept
{
#if USE_SMALL_POOLS == 1
	arena.tp0.mtx.lock();
	arena.tp1.mtx.lock();
	arena.tp2.mtx.lock();
	arena.tp3.mtx.lock();
	arena.tp4.mtx.lock();
	arena.tp5.mtx.lock();
#endif // USE_SMALL_POOLS
    
	arena.largePool[0].mtx.lock();
	arena.largePool[1].mtx.lock();
}

void MemoryArena::UnlockAll() noexcept
{
#if USE_SMALL_POOLS == 1
	arena.tp0.mtx.unlock();
	arena.tp1.mtx.unlock();
	arena.tp2.mtx.unlock();
	arena.tp3.mtx.unlock();
	arena.tp4.mtx.unlock();
	arena.tp5.mtx.unlock();
#endif // USE_SMALL_POOLS
    
    arena.largePool[0].mtx.unlock();
	arena.largePool[1].mtx.unlock();
}

bool MemoryArena::isInside(void* _Ptr) noexcept
{
#if USE_SMALL_POOLS == 0
	return (arena.largePool[0].isInside(_Ptr) || arena.largePool[1].isInside(_Ptr));
#else
	return (arena.tp0.isInside(_Ptr) || arena.tp1.isInside(_Ptr) ||
		    arena.tp2.isInside(_Ptr) || arena.tp3.isInside(_Ptr) ||
		    arena.tp4.isInside(_Ptr) || arena.tp5.isInside(_Ptr) ||
		    arena.largePool[0].isInside(_Ptr) || arena.largePool[1].isInside(_Ptr));
#endif // USE_SMALL_POOLS && USE_TEMPL_POOLS
}

// iei
