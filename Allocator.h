#pragma once
#include "MemoryArena.h"
#include <memory>

// В този хедър има САМО неща, необходими за интеграцията с std::allocator_traits

namespace andi
{
	template<class T>
	class allocator;

	template<>
	class allocator<void>
	{
	public:
		using value_type	= void;
		using pointer		= void*;
		using const_pointer	= const void*;
		template<class U> struct rebind { using other = allocator<U>; };
	};

	template<class T>
	class allocator
	{
	public:
		using value_type		= T;
		using pointer			= value_type*;
		using const_pointer		= const value_type*;
		using reference			= value_type&;
		using const_reference	= const value_type&;
		using size_type			= std::size_t;
		using difference_type	= std::ptrdiff_t;
		using is_always_equal	= std::true_type;

		using propagate_on_container_move_assignment = std::true_type;
		template<class U> struct rebind { using other = allocator<U>; };

		allocator() = default;
		allocator(const allocator&) = default;
		allocator& operator=(const allocator&) = default;
		template<class U>
		allocator(const allocator<U>&) noexcept;
		template<class U>
		allocator& operator=(const allocator<U>&) noexcept;

			  pointer address(		reference) const noexcept;
		const_pointer address(const_reference) const noexcept;

		pointer allocate(size_type, allocator<void>::const_pointer = nullptr);
		void deallocate(pointer, size_type = 0);

		template<class U, class... Args>
		void construct(U*, Args&&...);
		template<class U>
		void destroy(U*);

		size_type max_size() const noexcept;
	};

	template<class T>
	template<class U>
	allocator<T>::allocator(const allocator<U>& other) noexcept {}

	template<class T>
	template<class U>
	allocator<T>& allocator<T>::operator=(const allocator<U>& other) noexcept { return *this; }

	template<class T>
	auto allocator<T>::address(reference x) const noexcept -> pointer
	{
		return std::addressof(x);
	}

	template<class T>
	auto allocator<T>::address(const_reference x) const noexcept -> const_pointer
	{
		return std::addressof(x);
	}

	template<class T>
	auto allocator<T>::allocate(size_type n, allocator<void>::const_pointer) -> pointer
	{
		return pointer(MemoryArena::Allocate(n*sizeof(T)));
	}

	template<class T>
	void allocator<T>::deallocate(pointer ptr, size_type)
	{
		MemoryArena::Deallocate(ptr);
	}

	template<class T>
	template<class U, class ...Args>
	void allocator<T>::construct(U* ptr, Args&&... args)
	{
		::new ((void*)ptr) U(std::forward<Args>(args)...);
	}

	template<class T>
	template<class U>
	void allocator<T>::destroy(U* ptr)
	{
		ptr->~U();
	}

	template<class T>
	auto allocator<T>::max_size() const noexcept -> size_type
	{
		return MemoryArena::max_size()/sizeof(allocator<T>::value_type);
	}
}

template<class T1, class T2>
constexpr bool operator==(const andi::allocator<T1>& lhs, const andi::allocator<T2>& rhs)
{
	return true;
}

template<class T1, class T2>
constexpr bool operator!=(const andi::allocator<T1>& lhs, const andi::allocator<T2>& rhs)
{
	return false;
}

// iei