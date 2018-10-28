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
 - implement vassert() w/ DebugBreak()
 - make minimal andi::allocator (Bob Steagall 2017, 43:56)
 */

// For convenience: andi::string instead of std::string, andi::vector<int>, andi::map<...>, etc.
namespace andi {
    using string = std::basic_string<char, std::char_traits<char>, andi::allocator<char>>;

    template<class T>
    using vector = std::vector<T, andi::allocator<T>>;
    
    template<class Key, class Value, class Pred = std::less<Key>>
    using map = std::map<Key, Value, Pred, andi::allocator<std::pair<const Key, Value>>>;
}

// A simple benchmark + some helper functions
using std::chrono::microseconds;
void testRandomStringAllocation(size_t, size_t, size_t, size_t);
template<template<class> class Allocator>
microseconds singleTestTimer(const andi::vector<size_t>&);

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
    
    MemoryArena::PrintCondition();
    MemoryArena::Deinitialize();
}

void testRandomStringAllocation(size_t numReps, size_t nStrings, size_t minLength, size_t maxLength) {
    // numReps iterations of the following procedure: allocate nStrings
    // char* arrays with random length between minLength and maxLength,
    // and then deallocate about a quarter of them. Afterwards, allocate
    // again the previously deallocated, and stop the timer. The deallocation
    // of everything at the end of the function is not included in the time.
    std::mt19937 gen{ static_cast<unsigned>(std::chrono::steady_clock::now().time_since_epoch().count()) };
    std::uniform_int_distribution<size_t> distr(minLength, maxLength);
    andi::vector<std::pair<microseconds, microseconds>> times(numReps);
    andi::vector<size_t> lengths(nStrings);

    for (auto& p : times) {
        // generate the arrays lengths beforehand
        for (auto& len : lengths)
            len = distr(gen);
        // run a single test
        p = { singleTestTimer<andi::allocator>(lengths),
              singleTestTimer<std::allocator>(lengths) };
    }

    static andi::mutex coutmtx;
    andi::lock_guard lock{ coutmtx };
    std::cout << "Testing " << nStrings << " string allocations and ~" << nStrings / 4 << " reallocations...\n";
    std::cout << "String length between " << minLength << " and " << maxLength << ".\n";
    // Print the timings from the tests
    double a = 0., s = 0.;
    std::cout << "andi::allocator\tstd::allocator\tdifference\t(%)\n";
    for (const auto& p : times) {
        const double a_ = double(p.first.count()) / 1000.;
        const double s_ = double(p.second.count()) / 1000.;
        a += a_; s += s_;
        std::cout << "  " << a_ << "ms\t  " << s_ << "ms\t" << a_ - s_
            << "ms\t(" << 100 * (a_ - s_) / s_ << "%)\n";
    }
    a /= times.size();
    s /= times.size();
    std::cout << "Average:\n  " << a << "ms\t  " << s << "ms\t" << a - s << "ms\t(" << 100 * (a - s) / s << "%)\n";
    std::cout << "\n";
}

template<template<class> class Allocator>
microseconds singleTestTimer(const andi::vector<size_t>& lengths) {
    const size_t n = lengths.size();

    auto start = std::chrono::steady_clock::now();
    char** strings = Allocator<char*>().allocate(n);
    Allocator<char> al;
    for (size_t i = 0; i < n; i++)
        strings[i] = al.allocate(lengths[i]);
    
    for (size_t i = 0; i < n; i++)
        if (lengths[i] % 4 == 0)
            al.deallocate(strings[i], lengths[i]);
    
    for (size_t i = 0; i < n; i++)
        if (lengths[i] % 4 == 0)
            strings[i] = al.allocate(lengths[i]);
    
    auto end = std::chrono::steady_clock::now();

    for (size_t i = 0; i < n; i++)
        al.deallocate(strings[i], lengths[i]);
    Allocator<char*>().deallocate(strings, n);

    return std::chrono::duration_cast<microseconds>(end - start);
}

// iei
