#include "Allocator.h"
#include <thread>
// STL, used for comparison
#include <vector>
#include <map>
#include <string>
// for benchmarking
#include <utility>
#include <chrono>
#include <random>

/* TO-DO:
 - FIX THE SMALL POOLS (!)
 - add a method for system-allocated bytes
 - add Benchmarks.{h,cpp}
 - implement vassert()
 - make an Allocator interface: Initialize, Deinitialize, Print, Allocate, Deallocate, IsInside/Contains, MaxSize, UsefulSize
 - make minimal andi::allocator (Bob Steagall 2017, 43:56)
 - simplify time measurement (A chrono tutorial, 50:03)
 */

// Time measuring
using Clock = std::chrono::high_resolution_clock;
using TimePoint = Clock::time_point;
double milliseconds(const TimePoint& _Start, const TimePoint& _End) {
    return 1000 * std::chrono::duration_cast<std::chrono::duration<double>>(_End - _Start).count();
}

// For convenience: andi::string instead of std::string, andi::vector<int>, andi::map<...>, etc.
namespace andi
{
    using string = std::basic_string<char, std::char_traits<char>, andi::allocator<char>>;

    template<class T>
    using vector = std::vector<T, andi::allocator<T>>;
    
    template<class Key, class Value, class Pred = std::less<Key>>
    using map = std::map<Key, Value, Pred, andi::allocator<std::pair<const Key, Value>>>;
}

// A simple benchmark + some helper functions
void testRandomStringAllocation(size_t, size_t, size_t, size_t);
template<template<class> class Allocator>
double singleTestTimer(const andi::vector<size_t>&);
void printTestResults(const andi::vector<std::pair<double, double>>&);

int main() {
    MemoryArena::Initialize();

    // 1 thread: up to ~70% faster
    // 4 threads: up to ~40%
    std::vector<std::thread> ths;
    const int nthreads = 1; // the # of threads running in parallel
    for (int i = 0; i < nthreads; i++)
        ths.emplace_back(testRandomStringAllocation, 25, 500000, 20, 1000);

    for (auto& th : ths)
        th.join();
    
    MemoryArena::printCondition();
    MemoryArena::Deinitialize();
}

void testRandomStringAllocation(size_t _Repetitions, size_t _nStrings, size_t _MinLength, size_t _MaxLength)
{
    // _Repetitions iterations of the following procedure: allocate _nStrings
    // char* arrays with random length between _MinLength and _MaxLength,
    // and then deallocate about a quarter of them. Afterwards, allocate
    // again the previously deallocated, and stop the timer. The deallocation
    // of everything at the end of the function is not included in the time.
    andi::vector<std::pair<double, double>> times(_Repetitions);
    std::default_random_engine gen;
    std::uniform_int_distribution<size_t> distr(_MinLength, _MaxLength);
    andi::vector<size_t> lengths(_nStrings);
    static andi::mutex coutmtx;

    for (auto& p : times) {
        // generate the arrays lengths beforehand
        for (auto& len : lengths)
            len = distr(gen);
        // run a single test
        p = { singleTestTimer<andi::allocator>(lengths),
              singleTestTimer<std::allocator>(lengths) };
    }

    andi::lock_guard lock{ coutmtx };
    std::cout << "Testing " << _nStrings << " string allocations and ~" << _nStrings / 4 << " reallocations...\n";
    std::cout << "String length between " << _MinLength << " and " << _MaxLength << ".\n";
    printTestResults(times);
}

template<template<class> class Allocator>
double singleTestTimer(const andi::vector<size_t>& _Lengths) {
    const size_t n = _Lengths.size();
    andi::vector<int> free(n, 0);

    TimePoint start = Clock::now();
    char** strings = Allocator<char*>().allocate(n);
    Allocator<char> al;
    for (size_t i = 0; i < n; i++)
        strings[i] = al.allocate(_Lengths[i]);
    
    for (size_t i = 0; i < n; i++)
        if (_Lengths[i] % 4 == 0) {
            al.deallocate(strings[i], _Lengths[i]);
            free[i] = 1;
    }
    for (size_t i = 0; i < n; i++)
        if (free[i])
            strings[i] = al.allocate(_Lengths[i]);
    
    TimePoint end = Clock::now();

    for (size_t i = 0; i < n; i++)
        al.deallocate(strings[i], _Lengths[i]);
    Allocator<char*>().deallocate(strings, n);

    return milliseconds(start,end);
}

void printTestResults(const andi::vector<std::pair<double, double>>& _Times) {
    if (_Times.size() == 0)
        return;
    double a = 0., s = 0.;
    std::cout << "andi::allocator\tstd::allocator\tdifference\t(%)\n";
    for (const auto& p : _Times) {
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
