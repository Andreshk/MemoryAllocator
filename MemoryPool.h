#pragma once
#include "Commons.h"

// Структура, която ще съхранява необходимата информация за
// управлението на свободните суперблокове памет
struct Superblock
{
#if HPC_DEBUG == 1
	union
	{
		struct
		{
			uint16_t k;
			uint16_t free;
		};
		uint32_t blueprint;
	};
	uint32_t signature;
#else
	uint32_t k;
	uint32_t free;
#endif // HPC_DEBUG
	// всички член-данни дотук образуват т.нар. хедър
	// fun fact: на теория този хедър може да бъде намален до 7 _бита_ без таговете -> O(lglgn)
	Superblock* prev;
	Superblock* next;
};

// Глобални константи и дефиниции за управлявания с бъди алокатор memory pool
using byte = char;
using ptr_t = std::uintptr_t;	// неотрицателен целочислен тип за кастване от/към пойнтъри

// големината на хедърите на суперблоковете в байтове
constexpr size_t	headerSize = offsetof(Superblock, prev);
// логаритъм от най-малкия размер блок, който ще заделяме и управляваме
constexpr uint32_t	minBlockSizeLog = 5;
// максималният размер памет, която може да бъде "алокирана" наведнъж
constexpr size_t	allocatorMaxSize = (largePoolSize / 4) - headerSize;

// Sanity check за валидност на глобалните константи
static_assert(headerSize < 32 && largePoolSizeLog <= 63 && headerSize < (size_t(1) << minBlockSizeLog) &&
	          minBlockSizeLog >= 5 && minBlockSizeLog <= largePoolSizeLog && allocatorMaxSize < largePoolSize &&
	          allocatorMaxSize + headerSize - 1 < 0x100000000ui64, "Invalid global variable values!");

// forward declaration...
class MemoryArena;

/*
 - Паметта, която се подава на потребителя, се "алокира" от голямо адресно
 пространство (pool) с размер точна степен на двойката (примерно, 4GB).
 Това адресно пространство се заделя еднократно при конструиране и не се променя.
 - Системната информация за pool-а е таблица, в която за всяка стойност на наредената
 двойка (k,i) пазим кои са свободните суперблокове с размер 2^k-2^i байта.
 - Свободните суперблокове от един и същ размер се пазят в цикличен (!)
 двойносвързан списък, тъй като при merge се налага да премахваме суперблокове,
 които се намират на произволна позиция в списъка (а не веднага след главата му).
 Списъкът е цикличен и винаги непразен, за да се ускори добавянето/премахването на
 елемент в него (без проверки за нулеви указатели). Най-големият размер суперблок
 обхваща цялото адресно пространство и е с големина 2^largePoolSizeLog, което
 представяме като суперблок с размер 2^(largePoolSizeLog+1) - 2^largePoolSizeLog.
 - Освен тези списъци пазим за всяко k битвектор, в който всеки i-ти бит е вдигнат
 т.с.т.к. съществува празен суперблок с големина 2^k-2^i. Тези битвектори
 актуализираме в константно време при всяко заемане/освобождаване на суперблок
 и използваме, за да дадем на потребителя най-подходящия суперблок за
 количеството памет, което той е пожелал.
 - За всеки битвектор пазим и актуализираме кой е най-младшият вдигнат бит -
 използваме тази информация при търсенето на най-подходящ блок памет.
*/
class MemoryPool
{
	friend class MemoryArena;

private:
	byte* poolPtr;
	ptr_t virtualZero;
	Superblock freeBlocks[largePoolSizeLog + 2][largePoolSizeLog + 1];
	uint64_t bitvectors[largePoolSizeLog + 2];
	uint32_t leastSetBits[largePoolSizeLog + 2];
	andi::mutex mtx;

	MemoryPool() noexcept; // няма деструктор, разчитаме на Deinitialize
	void Reset() noexcept;
	void Initialize() noexcept;
	void Deinitialize() noexcept;

	void* Allocate(size_t) noexcept;
	void Deallocate(void*) noexcept(isRelease);
	static size_t max_size() noexcept;
	bool isInside(void*) const noexcept;
	void printCondition() const;
#if HPC_DEBUG == 1
	static void sign(Superblock*) noexcept;
	static uint32_t getSignature(Superblock*) noexcept;
	static bool isValidSignature(Superblock*) noexcept;
#endif // HPC_DEBUG

	void* allocateSuperblock(size_t) noexcept;
	void deallocateSuperblock(void*) noexcept;
	void insertFreeSuperblock(Superblock*) noexcept;
	void removeFreeSuperblock(Superblock*) noexcept;
	Superblock* findFreeSuperblock(uint32_t) const noexcept;
	Superblock* findBuddySuperblock(Superblock*) const noexcept;
	void recursiveMerge(Superblock*) noexcept;
	static void* toUserAddress(Superblock*) noexcept;
	static Superblock* fromUserAddress(void*) noexcept;
	ptr_t toVirtualOffset(Superblock*) const noexcept;
	Superblock* fromVirtualOffset(ptr_t) const noexcept;
	uint32_t calculateI(Superblock*) const noexcept;
	static uint32_t calculateJ(size_t) noexcept;

public:
	// не е позволено копиране/преместване на pool-ове
	MemoryPool(const MemoryPool&) = delete;
	MemoryPool& operator=(const MemoryPool&) = delete;
	MemoryPool(MemoryPool&&) = delete;
	MemoryPool& operator=(MemoryPool&&) = delete;
};

// Помощни математически функции
uint32_t min(uint32_t, uint32_t);
uint32_t max(uint32_t, uint32_t);
uint64_t max(uint64_t, uint64_t);
uint32_t leastSetBit(uint32_t);
uint32_t leastSetBit(uint64_t);
uint32_t fastlog2(uint32_t);
uint32_t fastlog2(uint64_t);

// iei