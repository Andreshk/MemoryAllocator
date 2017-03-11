#include "Allocator.h"
#include <thread>
// за STL
#include <vector>
#include <map>
#include <string>
// за benchmark-овете
#include <utility>
#include <chrono>
#include <random>

// За измерване на времето
using Clock = std::chrono::high_resolution_clock;
using TimePoint = Clock::time_point;
double milliseconds(const TimePoint& _Start, const TimePoint& _End)
{
	return 1000 * std::chrono::duration_cast<std::chrono::duration<double>>(_End - _Start).count();
}

// За удобство: andi::string на мястото на std::string, andi::vector<int>, andi::map<...> и т.н.
namespace andi
{
	using string = std::basic_string<char, std::char_traits<char>, andi::allocator<char>>;

	template<class T>
	using vector = std::vector<T, andi::allocator<T>>;
	
	template<class Key, class Value, class Pred = std::less<Key>>
	using map = std::map<Key, Value, Pred, andi::allocator<std::pair<const Key, Value>>>;
}

// Една проста бенчмаркваща функция + нейни помощни
void testRandomStringAllocation(size_t, size_t, size_t, size_t);
template<template<class> class Allocator>
double singleTestTimer(const andi::vector<size_t>&);
void printTestResults(const andi::vector<std::pair<double, double>>&);

int main()
{
	MemoryArena::Initialize();

	// 1 нишка: до ~70%
	// 4 нишки: до ~40%
	std::vector<std::thread> ths;
	for (int i = 0; i < 4; i++) // change the loop limit to change the # of threads running in parallel (obviously)
		ths.emplace_back(testRandomStringAllocation, 25, 500000, 20, 1000);

	for (auto& th : ths)
		th.join();
    
	MemoryArena::printCondition();
	MemoryArena::Deinitialize();
}

void testRandomStringAllocation(size_t _Repetitions, size_t _nStrings, size_t _MinLength, size_t _MaxLength)
{
	// Правят се _Repetitions на брой итерации на следната процедура:
	// заделят сe _nStrings на брой char* масива (в голям char**)
	// с дължина произволно число между _MinLength и _MaxLength,
	// след което около 1/4 от тях се освобождават. После освободените
	// се заделят наново и се записва времето, което е отнела цялата процедура.
	// Естествено, после всичко се освобождава, но това не влиза във времето.
	andi::vector<std::pair<double, double>> times(_Repetitions);
	std::default_random_engine gen;
	std::uniform_int_distribution<size_t> distr(_MinLength, _MaxLength);
	andi::vector<size_t> lengths(_nStrings);
    static andi::mutex coutmtx;

	for (auto& p : times)
	{
		// генерира нов масив от дължини
		for (auto& len : lengths)
			len = distr(gen);
		// пуска единичния тест с новогенерираните дължини
		p = { singleTestTimer<andi::allocator>(lengths),
			  singleTestTimer<std::allocator>(lengths) };
	}

	coutmtx.lock();
	std::cout << "Testing " << _nStrings << " string allocations and ~" << _nStrings / 4 << " reallocations...\n";
	std::cout << "String length between " << _MinLength << " and " << _MaxLength << ".\n";
	printTestResults(times);
	coutmtx.unlock();
}

template<template<class> class Allocator>
double singleTestTimer(const andi::vector<size_t>& _Lengths)
{
	const size_t n = _Lengths.size();
	andi::vector<int> free(n, 0);

	TimePoint start = Clock::now();
	char** strings = Allocator<char*>().allocate(n);
	Allocator<char> al;
	for (size_t i = 0; i < n; i++)
		strings[i] = al.allocate(_Lengths[i]);
	
	for (size_t i = 0; i < n; i++) if (_Lengths[i] % 4 == 0)
	{
		al.deallocate(strings[i], _Lengths[i]);
		free[i] = 1;
	}
	for (size_t i = 0; i < n; i++) if (free[i])
		strings[i] = al.allocate(_Lengths[i]);
	
	TimePoint end = Clock::now();

	for (size_t i = 0; i < n; i++)
		al.deallocate(strings[i], _Lengths[i]);
	Allocator<char*>().deallocate(strings, n);

	return milliseconds(start,end);
}

void printTestResults(const andi::vector<std::pair<double, double>>& _Times)
{
	if (_Times.size() == 0)
		return;
	double a = 0., s = 0.;
	std::cout << "andi::allocator\tstd::allocator\tdifference\t(%)\n";
	for (const auto& p : _Times)
	{
		a += p.first;
		s += p.second;
		std::cout << "  " << p.first << "ms\t  " << p.second << "ms\t" << p.first - p.second
				  << "ms\t(" << 100 * (p.first - p.second) / p.second << "%)\n";
	}
	a /= _Times.size();
	s /= _Times.size();
	std::cout << "Average:\n  " << a << "ms\t  " << s << "ms\t" << a - s << "ms\t(" << 100 * (a - s) / s << "%)\n";
	std::cout << "\n";
}

// iei