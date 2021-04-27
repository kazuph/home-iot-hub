#ifndef CONST_MAP_HPP
#define CONST_MAP_HPP

#include <iterator>
#include <cstdint>
#include <algorithm>

namespace hub::utils
{
    template<typename T>
    class _const_map_iterator
    {
    public:

        using iterator_category     = std::bidirectional_iterator_tag;
        using value_type            = T;
        using difference_type       = ptrdiff_t;
        using pointer               = T*;
        using reference             = T&;

        constexpr _const_map_iterator() noexcept : ptr{ nullptr } {}

        constexpr explicit _const_map_iterator(pointer ptr) noexcept : ptr{ ptr } {}

        constexpr _const_map_iterator(const _const_map_iterator& other) noexcept : ptr{ other.ptr } {}

        ~_const_map_iterator() = default;

        constexpr _const_map_iterator& operator=(const _const_map_iterator& other) noexcept
        {
            ptr = other.ptr;
            return *this;
        }

        [[nodiscard]] constexpr bool operator==(const _const_map_iterator& other) const noexcept
        {
            return (ptr == other.ptr);
        }

        [[nodiscard]] constexpr bool operator!=(const _const_map_iterator& other) const noexcept
        {
            return (ptr != other.ptr);
        }

        [[nodiscard]] constexpr pointer operator->() const noexcept
        {
            return ptr;
        }

        [[nodiscard]] constexpr reference operator*() const noexcept
        {
            return *(operator->());
        }

        constexpr _const_map_iterator& operator++() noexcept
        {
            ++ptr;
            return *this;
        }

        constexpr _const_map_iterator operator++(int) noexcept
        {
            _const_map_iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        constexpr _const_map_iterator& operator--() noexcept
        {
            --ptr;
            return *this;
        }

        constexpr _const_map_iterator operator--(int) noexcept
        {
            _const_map_iterator tmp = *this;
            --(*this);
            return tmp;
        }

    private:

        pointer ptr;
    };

    template<typename Key, typename T, std::size_t Size, typename Compare = std::less<Key>>
    struct const_map
    {
        using key_type                  = Key;
        using mapped_type               = T;
        using key_compare               = Compare;
        using value_type                = std::pair<const key_type, mapped_type>;
        using size_type                 = std::size_t;
        using difference_type           = std::ptrdiff_t;
        using pointer                   = value_type*;
        using const_pointer             = const pointer;
        using reference                 = value_type&;
        using const_reference           = const reference;
        using iterator                  = _const_map_iterator<value_type>;
        using const_iterator            = _const_map_iterator<const value_type>;
        using reverse_iterator          = std::reverse_iterator<iterator>;
        using const_reverse_iterator    = std::reverse_iterator<const_iterator>;

        value_type _data[Size];

        [[nodiscard]] constexpr size_type size() const noexcept
        {
            return Size;
        }

        [[nodiscard]] constexpr pointer data() noexcept
        {
            return _data;
        }

        [[nodiscard]] constexpr pointer data() const noexcept
        {
            return _data;
        }

        [[nodiscard]] constexpr iterator begin() noexcept
        {
            return iterator(_data);
        }

        [[nodiscard]] constexpr const_iterator begin() const noexcept
        {
            return const_iterator(_data);
        }

        [[nodiscard]] constexpr const_iterator cbegin() const noexcept
        {
            return begin();
        }

        [[nodiscard]] constexpr reverse_iterator rbegin() noexcept
        {
            return reverse_iterator(begin());
        }

        [[nodiscard]] constexpr const_reverse_iterator rbegin() const noexcept
        {
            return const_reverse_iterator(cbegin());
        }

        [[nodiscard]] constexpr iterator end() noexcept
        {
            return iterator(_data + Size);
        }

        [[nodiscard]] constexpr const_iterator end() const noexcept
        {
            return const_iterator(_data + Size);
        }

        [[nodiscard]] constexpr const_iterator cend() const noexcept
        {
            return end();
        }

        [[nodiscard]] constexpr reverse_iterator rend() noexcept
        {
            return reverse_iterator(end());
        }

        [[nodiscard]] constexpr const_reverse_iterator rend() const noexcept
        {
            return const_reverse_iterator(cend());
        }

        [[nodiscard]] constexpr bool is_sorted() const noexcept
        {
            key_compare pred{};
            auto left{ cbegin() };
            auto right{ std::next(left) };

            while (right != cend())
            {
                if (!pred(left->first, right->first))
                {
                    return false;
                }

                left = right++;
            }

            return true;
        }

        [[nodiscard]] constexpr iterator find(const key_type& key) noexcept
        {
            for (auto it = begin(); it != end(); ++it)
            {
                if (it->first == key)
                {
                    return it;
                }
            }

            return end();
        }

        [[nodiscard]] constexpr const_iterator find(const key_type& key) const noexcept
        {
            for (auto it = cbegin(); it != cend(); ++it)
            {
                if (it->first == key)
                {
                    return it;
                }
            }

            return cend();
        }

        [[nodiscard]] constexpr mapped_type& operator[](const key_type& key) noexcept
        {
            return (find(key))->second;
        }

        [[nodiscard]] constexpr const mapped_type& operator[](const key_type& key) const noexcept
        {
            return (find(key))->second;
        }

        [[nodiscard]] constexpr mapped_type& at(const key_type& key)
        {
            auto iter = find(key);

            if (iter == end())
            {
                std::abort();
            }

            return iter->second;
        }

        [[nodiscard]] constexpr const mapped_type& at(const key_type& key) const
        {
            auto iter = find(key);

            if (iter == cend())
            {
                std::abort();
            }

            return iter->second;
        }

        [[nodiscard]] constexpr bool contains(const key_type& key) const noexcept
        {
            return (find(key) != cend());
        }
    };
}

#endif