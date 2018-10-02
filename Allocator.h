#pragma once
#include "MemoryArena.h"
#include <memory>

// This header contains only the methods, needed to integrate with std::allocator_traits

namespace andi
{
    template<class T>
    class allocator;

    template<>
    class allocator<void> {
    public:
        using value_type    = void;
        using pointer       = void*;
        using const_pointer = const void*;
        template<class U> struct rebind { using other = allocator<U>; };
    };

    template<class T>
    class allocator {
    public:
        using value_type        = T;
        using pointer           = value_type*;
        using const_pointer     = const value_type*;
        using reference         = value_type&;
        using const_reference   = const value_type&;
        using size_type         = std::size_t;
        using difference_type   = std::ptrdiff_t;
        using is_always_equal   = std::true_type;

        using propagate_on_container_move_assignment = std::true_type;
        template<class U> struct rebind { using other = allocator<U>; };

        allocator() = default;
        allocator(const allocator&) = default;
        allocator& operator=(const allocator&) = default;
        template<class U>
        allocator(const allocator<U>&) noexcept {};
        template<class U>
        allocator& operator=(const allocator<U>&) noexcept { return *this; };

              pointer address(      reference x) const noexcept { return std::addressof(x); }
        const_pointer address(const_reference x) const noexcept { return std::addressof(x); }

        pointer allocate(size_type n, allocator<void>::const_pointer = nullptr) {
            return pointer(MemoryArena::Allocate(n * sizeof(T)));
        }
        void deallocate(pointer ptr, size_type = 0) {
            MemoryArena::Deallocate(ptr);
        }

        template<class U, class... Args>
        void construct(U* ptr, Args&&... args) {
            ::new ((void*)ptr) U(std::forward<Args>(args)...);
        }
        template<class U>
        void destroy(U* ptr) {
            ptr->~U();
        }

        size_type max_size() const noexcept {
            return MemoryArena::MaxSize() / sizeof(allocator<T>::value_type);
        }
    };
}

template<class T1, class T2>
constexpr bool operator==(const andi::allocator<T1>& lhs, const andi::allocator<T2>& rhs) {
    return true;
}

template<class T1, class T2>
constexpr bool operator!=(const andi::allocator<T1>& lhs, const andi::allocator<T2>& rhs) {
    return false;
}

// iei
