#pragma once

#include <array>
#include <memory>
#include <iterator>
#include <vector>

#include "Logging/Assert.h"

namespace enger
{
    template<typename T, std::size_t Capacity>
    requires std::is_default_constructible_v<T>
    class InplaceVector final
    {
    public:
        class Iterator final
        {
        public:
            using iterator_category = std::contiguous_iterator_tag;
            using difference_type = std::ptrdiff_t;
            using value_type = T;
            using element_type = T;
            using pointer = T*;
            using reference = T&;
            using iterator_type = Iterator;


            constexpr Iterator() noexcept = default;
            constexpr Iterator(pointer ptr) noexcept
                : m_Ptr(ptr)
            {}


            constexpr reference operator*() const noexcept { return *m_Ptr; }
            constexpr reference operator*() noexcept { return *m_Ptr; }
            constexpr pointer operator->() const noexcept { return m_Ptr; }
            constexpr pointer operator->() noexcept { return m_Ptr; }

            constexpr reference operator[](difference_type n) const noexcept { return m_Ptr[n]; }
            constexpr reference operator[](difference_type n) noexcept { return m_Ptr[n]; }
            constexpr pointer get() const noexcept { return m_Ptr; }

            constexpr Iterator& operator++() noexcept { ++m_Ptr; return *this; }
            constexpr Iterator operator++(int) noexcept { auto tmp = *this; ++m_Ptr; return tmp; }
            constexpr Iterator& operator--() noexcept { --m_Ptr; return *this; }
            constexpr Iterator operator--(int) noexcept { auto tmp = *this; --m_Ptr; return tmp; }
            constexpr Iterator& operator+=(difference_type n) noexcept { m_Ptr += n; return *this; }
            constexpr Iterator& operator-=(difference_type n) noexcept { m_Ptr -= n; return *this; }

            friend constexpr auto operator<=>(Iterator, Iterator) noexcept = default;
            friend constexpr Iterator operator+(Iterator it, difference_type d) noexcept { return Iterator(it.m_Ptr + d); }
            friend constexpr Iterator operator+(difference_type d, Iterator it) noexcept { return Iterator(it.m_Ptr + d); }
            friend constexpr Iterator operator-(Iterator it, difference_type d) noexcept { return Iterator(it.m_Ptr - d); }
            friend constexpr Iterator operator-(difference_type d, Iterator it) noexcept { return Iterator(it.m_Ptr - d); }
            friend constexpr Iterator operator+(Iterator lhs, Iterator rhs) noexcept { return Iterator(lhs.m_Ptr + rhs.m_Ptr); }
            friend constexpr difference_type operator-(Iterator lhs, Iterator rhs) noexcept { return lhs.m_Ptr - rhs.m_Ptr; }

        private:
            pointer m_Ptr = nullptr;
        };
        static_assert(std::contiguous_iterator<Iterator>);

        class ConstIterator final
        {
        public:
            using iterator_category = std::contiguous_iterator_tag;
            using difference_type = std::ptrdiff_t;
            using value_type = T;
            using element_type = const T;
            using pointer = const T*;
            using reference = const T&;
            using iterator_type = ConstIterator;


            constexpr ConstIterator() noexcept = default;
            constexpr ConstIterator(pointer ptr) noexcept
                : m_Ptr(ptr)
            {}


            constexpr reference operator*() const noexcept { return *m_Ptr; }
            constexpr pointer operator->() const noexcept { return m_Ptr; }

            constexpr reference operator[](difference_type n) const noexcept { return m_Ptr[n]; }
            constexpr pointer get() const noexcept { return m_Ptr; }

            constexpr ConstIterator& operator++() noexcept { return *this; }
            constexpr ConstIterator operator++(int) noexcept { auto tmp = *this; return tmp; }
            constexpr ConstIterator& operator--() noexcept { return *this; }
            constexpr ConstIterator operator--(int) noexcept { auto tmp = *this; return tmp; }
            constexpr ConstIterator& operator+=(difference_type n) noexcept { m_Ptr += n; return *this; }
            constexpr ConstIterator& operator-=(difference_type n) noexcept { m_Ptr -= n; return *this; }

            friend constexpr auto operator<=>(ConstIterator, ConstIterator) noexcept = default;
            friend constexpr ConstIterator operator+(ConstIterator it, difference_type d) noexcept { return ConstIterator(it.m_Ptr + d); }
            friend constexpr ConstIterator operator+(difference_type d, ConstIterator it) noexcept { return ConstIterator(it.m_Ptr + d); }
            friend constexpr ConstIterator operator-(ConstIterator it, difference_type d) noexcept { return ConstIterator(it.m_Ptr - d); }
            friend constexpr ConstIterator operator-(difference_type d, ConstIterator it) noexcept { return ConstIterator(it.m_Ptr - d); }
            friend constexpr ConstIterator operator+(ConstIterator lhs, ConstIterator rhs) noexcept { return ConstIterator(lhs.m_Ptr + rhs.m_Ptr); }
            friend constexpr difference_type operator-(ConstIterator lhs, ConstIterator rhs) noexcept { return lhs.m_Ptr - rhs.m_Ptr; }
            
        private:
            pointer m_Ptr = nullptr;
        };
        static_assert(std::contiguous_iterator<ConstIterator>);

        constexpr InplaceVector() = default;
        constexpr explicit InplaceVector(std::size_t size) noexcept
            : m_Size(size)
        {
            m_Data.fill(T{});
        }
        constexpr InplaceVector(std::size_t size, T value) noexcept
            : m_Size(size)
        {
            m_Data.fill(value);
        }

        constexpr InplaceVector(std::vector<T> vector) noexcept
            : m_Size(vector.size())
        {
            EASSERT(Capacity >= vector.size());
            std::copy(vector.begin(), vector.end(), m_Data.begin());
        }
        constexpr InplaceVector(const std::array<T, Capacity>& array) noexcept
            : m_Data(array), m_Size(Capacity)
        {}
        constexpr InplaceVector(std::array<T, Capacity>&& array) noexcept
            : m_Data(std::move(array)), m_Size(Capacity)
        {}

        constexpr InplaceVector(std::initializer_list<T> list) noexcept
            : m_Size(list.size())
        {
            EASSERT(Capacity >= list.size());
            std::copy(list.begin(), list.end(), m_Data.begin());
        }

        [[nodiscard]] constexpr T& operator[](std::size_t index) noexcept { return m_Data[index]; }
        [[nodiscard]] constexpr const T& operator[](std::size_t index) const noexcept { return m_Data[index]; }
        [[nodiscard]] constexpr T* data() noexcept { return m_Data.data(); }
        [[nodiscard]] constexpr const T* data() const noexcept { return m_Data.data(); }
        [[nodiscard]] constexpr std::size_t size() const noexcept { return m_Size; }
        [[nodiscard]] static constexpr std::size_t capacity() noexcept { return Capacity; }

        constexpr void resize(std::size_t size) noexcept
        {
            EASSERT(Capacity >= size);
            m_Size = size;
            m_Data.fill(T{});
        }
        constexpr void resize(std::size_t size, T value) noexcept
        {
            EASSERT(Capacity >= size);
            m_Size = size;
            m_Data.fill(value);
        }

        [[nodiscard]] constexpr bool empty() const noexcept { return m_Size == 0; }

        constexpr void push_back(const T& value) noexcept
        {
            EASSERT(Capacity > m_Size);
            m_Data[m_Size++] = value;
        }
        constexpr void push_back(T&& value) noexcept
        {
            EASSERT(Capacity > m_Size);
            m_Data[m_Size++] = std::move(value);
        }

        template<typename ...Args>
        constexpr void emplace_back(Args&&... args) noexcept
        {
            if consteval
            {
                static_assert(Capacity > m_Size);
            }
            else
            {
                EASSERT(Capacity > m_Size);
            }
            m_Data[m_Size++] = T(std::forward<Args>(args)...);
        }

        constexpr T pop_back() noexcept
        {
            if consteval
            {
                static_assert(m_Size > 0);
            }
            else
            {
                EASSERT(m_Size > 0);
            }
            return m_Data[--m_Size];
        }

        constexpr void clear() { m_Size = 0; }
        constexpr void swap(InplaceVector& other) noexcept
        {
            std::swap(m_Data, other.m_Data);
            std::swap(m_Size, other.m_Size);
        }

        constexpr Iterator begin() { return Iterator(m_Data.data()); }
        constexpr Iterator end() { return Iterator(&m_Data[m_Size]); }
        constexpr ConstIterator begin() const { return ConstIterator(m_Data.data()); }
        constexpr ConstIterator end() const { return ConstIterator(&m_Data[m_Size]); }
        constexpr ConstIterator cbegin() const { return begin(); }
        constexpr ConstIterator cend() const { return end(); }
        constexpr Iterator rbegin() { return end() - 1; }
        constexpr Iterator rend() { return begin() - 1; }
        constexpr ConstIterator rbegin() const { return end() - 1; }
        constexpr ConstIterator rend() const { return begin() - 1; }
        constexpr ConstIterator crbegin() const { return rbegin(); }
        constexpr ConstIterator crend() const { return rend(); }

        constexpr operator std::span<T>() const noexcept { return std::span<T>{m_Data.data(), m_Size}; }
        constexpr operator std::span<const T>() const noexcept { return std::span<const T>{m_Data.data(), m_Size}; }

    private:
        std::array<T, Capacity> m_Data{};
        std::size_t m_Size = 0;
    };
}
