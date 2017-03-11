#include "MemoryPool.h"

MemoryPool::MemoryPool() noexcept
{
	Reset();
}

void MemoryPool::Reset() noexcept
{
	poolPtr = nullptr;
	virtualZero = 0;
	for (uint32_t k = 0; k < largePoolSizeLog + 2; k++)
	{
		for (uint32_t i = 0; i < largePoolSizeLog + 1; i++)
		{
			freeBlocks[k][i].prev = nullptr;
			freeBlocks[k][i].next = nullptr;
		}
		bitvectors[k] = 0;
		leastSetBits[k] = 0;
	}
}

void MemoryPool::Initialize() noexcept
{
	const size_t poolSize = size_t(1) << largePoolSizeLog;
	// заделяме еднократно памет за целия pool
	poolPtr = (byte*)andi::aligned_malloc(poolSize + 32, 32);
	virtualZero = ptr_t(poolPtr) + 32 - headerSize;
	// инициализираме системната информация
	for (uint32_t k = 0; k < largePoolSizeLog + 2; k++)
	{
		for (uint32_t i = 0; i < largePoolSizeLog + 1; i++)
		{
			freeBlocks[k][i].prev = &freeBlocks[k][i];
			freeBlocks[k][i].next = &freeBlocks[k][i];
			// няма нужда да поддържаме free,k,i
		}
		bitvectors[k] = 0ui64;
		leastSetBits[k] = 64U;
	}
	// инициализираме и добавяме първоначалния суперблок в сист.инфо
	Superblock* sblk = (Superblock*)virtualZero;
	sblk->free = 1;
	sblk->k = largePoolSizeLog + 1;
#if HPC_DEBUG == 1
	sign(sblk);
#endif
	insertFreeSuperblock(sblk);
}

void MemoryPool::Deinitialize() noexcept
{
	andi::aligned_free(poolPtr);
	Reset();
}

void* MemoryPool::Allocate(size_t n) noexcept
{
	if (n > allocatorMaxSize)
		return nullptr;
	mtx.lock();
	void* ptr = allocateSuperblock(n);
	mtx.unlock();
	return ptr;
}

void MemoryPool::Deallocate(void* _Ptr) noexcept(isRelease)
{
	mtx.lock();
#if HPC_DEBUG == 1
	if (!isValidSignature(fromUserAddress(_Ptr)))
	{
		mtx.unlock();
		throw std::runtime_error("MemoryArena: Pointer is either already freed or is not the one, returned to user!\n");
	}
#endif // HPC_DEBUG
	deallocateSuperblock(_Ptr);
	mtx.unlock();
}

size_t MemoryPool::max_size() noexcept
{
	return allocatorMaxSize;
}

bool MemoryPool::isInside(void* _Ptr) const noexcept
{
	return _Ptr >= poolPtr && _Ptr < (poolPtr + largePoolSize + 32);
}

void MemoryPool::printCondition() const
{
	std::cout << "Pool address: 0x" << std::hex << (void*)poolPtr << std::dec << "\n";
	std::cout << "Pool size:  " << largePoolSize << " bytes.\n";
	std::cout << "Free superblocks of type (k,i):\n";
	size_t counter, free_space = 0;
	for (uint32_t k = 0; k < largePoolSizeLog + 2; k++)
	for (uint32_t i = 0; i < largePoolSizeLog + 1; i++)
	{
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
void MemoryPool::sign(Superblock* _Ptr) noexcept
{
	_Ptr->signature = getSignature(_Ptr);
}

uint32_t MemoryPool::getSignature(Superblock* _Ptr) noexcept
{
	return (~_Ptr->blueprint) ^ uint32_t(ptr_t(_Ptr) >> 8);
}

bool MemoryPool::isValidSignature(Superblock* _Ptr) noexcept
{
	/* Вероятността за произволен адрес да се получи валидна сигнатура
	 * (при еднобайтови enum-и) e 1/65536 * 28/65536 * 1/2^32,
	 * което е приблизително 1 на 600,000,000,000,000,000 (!)
	 * Нещо повече, т.к. самият адрес участва в сигнатурата, то при всяко
	 * следващо стартиране на програмата вероятността се умножава отново по
	 * това число - 1 на 10^35, 1 на 10^53,... въобще още след първото пускане
	 * можем да сме спокойни, че тази вероятност е практически нула. */ 
	return _Ptr->free==0 && _Ptr->k > minBlockSizeLog &&
		_Ptr->k <= largePoolSizeLog + 1 && _Ptr->signature == getSignature(_Ptr);
}
#endif // HPC_DEBUG

void* MemoryPool::allocateSuperblock(size_t n) noexcept
{
	const uint32_t j = calculateJ(n);
	Superblock* sblk = findFreeSuperblock(j);
	if (sblk == nullptr)
		return nullptr;

	// премахваме суперблока, после ще добавим отделно неговите парчета
	removeFreeSuperblock(sblk);
	const uint32_t old_k = sblk->k;
	const uint32_t old_i = calculateI(sblk);

	// ако трябва разбиване на по-малки блокове
	if (old_i > j)
	{
		// разбиваме на три малки суперблока
		sblk->free = 0;
		sblk->k = j + 1;

		// ъпдейтваме и системната информация за съществуването им
		Superblock* block1 = (Superblock*)(ptr_t(sblk) + (ptr_t(1) << j));
		block1->free = 1;
		block1->k = old_i;
#if HPC_DEBUG == 1
		sign(block1);
#endif
		insertFreeSuperblock(block1);

		if (old_k != old_i + 1)
		{
			Superblock* block2 = (Superblock*)(ptr_t(sblk) + (ptr_t(1) << old_i));
			block2->free = 1;
			block2->k = old_k;
#if HPC_DEBUG == 1
			sign(block2);
#endif
			insertFreeSuperblock(block2);
		}

		// директно връщаме sblk + отместването;
#if HPC_DEBUG == 1
		sign(sblk);
#endif
		return toUserAddress(sblk);
	}

	// изчисли къде в суперблока трябва да е блокa за потребителя - addr
	Superblock* addr = (Superblock*)(ptr_t(sblk) + (ptr_t(1) << j) - (ptr_t(1) << old_i));
	// маркирай като зает, запиши неговите k,i
	addr->free = 0;
	addr->k = j + 1;

	// ако трябва ляв супер блок
	if (j > old_i)
	{
		// полученият суперблок остава свободен, но актуализираме само неговото k
		sblk->k = j;
#if HPC_DEBUG == 1
		sign(sblk);
#endif
		// ъпдейтваме системната информация
		insertFreeSuperblock(sblk);
	}
	// ако трябва десен суперблок
	if (j < old_k - 1)
	{
		// маркираме го като свободен, актуализираме неговите k,i
		Superblock* rblock = (Superblock*)(ptr_t(addr) + (ptr_t(1) << j));
		rblock->free = 1;
		rblock->k = old_k;
#if HPC_DEBUG == 1
		sign(rblock);
#endif
		// ъпдейтваме системната информация
		insertFreeSuperblock(rblock);
	}

	//връщаме получения addr + отместване
#if HPC_DEBUG == 1
	sign(addr);
#endif
	return toUserAddress(addr);
}

void MemoryPool::deallocateSuperblock(void* _Ptr) noexcept
{
	// маркира текущия блок като свободен и започва
	// да го слива рекурсивно нагоре
	Superblock* sblk = fromUserAddress(_Ptr);
	sblk->free = 1;
	recursiveMerge(sblk);
}

void MemoryPool::insertFreeSuperblock(Superblock* _Ptr) noexcept
{
	// добавяме този суперблок към съответния си списък в сист.инфо
	const uint32_t k = _Ptr->k;
	const uint32_t i = calculateI(_Ptr);
	_Ptr->next = freeBlocks[k][i].next;
	freeBlocks[k][i].next = _Ptr;
	_Ptr->prev = &freeBlocks[k][i]; // == ptr->next->prev
	_Ptr->next->prev = _Ptr;
	// ъпдейт на битвектора, че със сигурност съществува
	// празен суперблок с този размер, т.е. вдигаме i-тия бит на k-тия вектор
	bitvectors[k] |= (1ui64 << i);
	leastSetBits[k] = leastSetBit(bitvectors[k]);	
}

void MemoryPool::removeFreeSuperblock(Superblock* _Ptr) noexcept
{
	// премахваме суперблока от системната информация
	_Ptr->prev->next = _Ptr->next;
	_Ptr->next->prev = _Ptr->prev;
	//ptr->next = ptr->prev = nullptr;
	const uint32_t k = _Ptr->k;
	const uint32_t i = calculateI(_Ptr);
	// ако няма останали суперблокове от размер (k,i), т.е.
	// в двусвързания списък е останала само главата му,
	// изчистваме i-тия бит на k-тия битвектор
	if (freeBlocks[k][i].next == &freeBlocks[k][i])
	{
		bitvectors[k] &= ~(1ui64 << i);
		leastSetBits[k] = leastSetBit(bitvectors[k]);
	}
}

Superblock* MemoryPool::findFreeSuperblock(uint32_t j) const noexcept
{
	uint32_t min_i = 64, min_k = 0;
	for (uint32_t k = j + 1; k < largePoolSizeLog + 2; k++) if (leastSetBits[k] < min_i)
	{
		min_i = leastSetBits[k];
		min_k = k;
	}
	if (min_i == 64)
		return nullptr;
	return freeBlocks[min_k][min_i].next;
}

Superblock* MemoryPool::findBuddySuperblock(Superblock* _Ptr) const noexcept
{
	// за да открием бъдито на даден блок, флипваме i+1-вия бит на виртуалния му адрес
	return fromVirtualOffset(toVirtualOffset(_Ptr) ^ (ptr_t(1) << calculateI(_Ptr)));
}

void MemoryPool::recursiveMerge(Superblock* _Ptr) noexcept
{
	// Сливаме суперблокове само когато са изпълнени следните три критерия:
	// - ако все още има какво да сливаме (т.е. не сме освободили целия pool)
	// - ако бъдито на текущия блок е свободно
	// - ако бъдито е с подходящ размер: текущият блок е с размер 2^i, а бъдито 2^k-2^i за някое k
	// В противен случай добавяме насъбраната досега свободна памет като обикновен суперблок
	// (той ще бъде с размер 2^j за някое j)
	Superblock* buddy = findBuddySuperblock(_Ptr);
	if ((ptr_t(_Ptr) == virtualZero && _Ptr->k == largePoolSizeLog + 1) ||
		buddy->free == 0 || calculateI(_Ptr) != calculateI(buddy))
	{
#if HPC_DEBUG == 1
		sign(_Ptr);
#endif
		insertFreeSuperblock(_Ptr);
		return;
	}
	// ще има сливане, махаме бъдито от сист.инфо
	removeFreeSuperblock(buddy);
	const uint32_t buddy_k = buddy->k;	// старото k
	// обединяваме двете бъдита в блок с големина 2^k (т.е. 2^(k+1) - 2^k)
	if (buddy < _Ptr)
		_Ptr = buddy;
	_Ptr->k = buddy_k + 1;
	// продължаваме нагоре със сливането
	recursiveMerge(_Ptr);
}

void* MemoryPool::toUserAddress(Superblock* _Ptr) noexcept
{
	return (void*)(ptr_t(_Ptr) + headerSize);
}

Superblock* MemoryPool::fromUserAddress(void* _Ptr) noexcept
{
	return (Superblock*)(ptr_t(_Ptr) - headerSize);
}

ptr_t MemoryPool::toVirtualOffset(Superblock* _Ptr) const noexcept
{
	return ptr_t(_Ptr) - virtualZero;
}

Superblock* MemoryPool::fromVirtualOffset(ptr_t _Offset) const noexcept
{
	return (Superblock*)(virtualZero + _Offset);
}

uint32_t MemoryPool::calculateI(Superblock* _Ptr) const noexcept
{
	return min(leastSetBit(toVirtualOffset(_Ptr)), _Ptr->k - 1);
}

uint32_t MemoryPool::calculateJ(size_t n) noexcept
{
	// това кастване е безобидно, т.с.т.к. allocatorMaxSize се събира в 32 бита
	return max(fastlog2(uint32_t(n + headerSize - 1)) + 1, minBlockSizeLog);
}

uint32_t min(uint32_t a, uint32_t b)
{
	return (a < b) ? a : b;
}

uint32_t max(uint32_t a, uint32_t b)
{
	return (a > b) ? a : b;
}

uint64_t max(uint64_t a, uint64_t b)
{
	return (a > b) ? a : b;
}

uint32_t leastSetBit(uint32_t x)
{
	static const uint32_t DeBruijnLeastSetBit[32] =
	{
		0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4, 8,
		31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6, 11, 5, 10, 9
	};
	return DeBruijnLeastSetBit[((x & (~x + 1U)) * 0x077CB531U) >> 27];
}

uint32_t leastSetBit(uint64_t x)
{
	if (x & 0xFFFFFFFFui64)
		return leastSetBit(uint32_t(x & 0xFFFFFFFFui64));
	else if (x != 0)
		return leastSetBit(uint32_t(x >> 32)) + 32;
	else
		return 64;
}

uint32_t fastlog2(uint32_t x) // изчислява floor(log2(x)) за всяко х
{
	static const uint32_t DeBruijnLog2Inexact[32] =
	{
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

uint32_t fastlog2(uint64_t x) // изчислява floor(log2(x)) за всяко x
{
	if (x < 0x100000000ui64)
		return fastlog2(uint32_t(x));
	else
		return 32 + fastlog2(uint32_t(x >> 32));
}

// iei