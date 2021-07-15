#ifndef HUB_UTILS_MAC_HPP
#define HUB_UTILS_MAC_HPP

#include <cassert>
#include <array>
#include <algorithm>
#include <cstdint>
#include <string>
#include <charconv>
#include <type_traits>
#include <iterator>

#include "fmt/core.h"

#include "esp_log.h"

namespace hub::utils
{
    class mac
    {
    public:

        static constexpr std::size_t MAC_SIZE{ 6 };
        static constexpr std::size_t MAC_STR_SIZE{ 17 }; // Size without null termination

        template<typename OutIt>
        inline void to_charbuff(OutIt first) const noexcept
        {
            static_assert(std::is_same_v<typename std::iterator_traits<OutIt>::value_type, char>, "Iterator value type must be uint8_t.");
            fmt::format_to(
                first,
                "{:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}",
                m_address[0],
                m_address[1],
                m_address[2],
                m_address[3],
                m_address[4],
                m_address[5]
            );
        }

        template<typename InIt>
        mac(InIt first, InIt last) :
            m_address{  }
        {
            static_assert(std::is_same_v<typename std::iterator_traits<InIt>::value_type, uint8_t>, "Iterator value type must be uint8_t.");
            assert(std::distance(first, last) == MAC_SIZE);
            std::copy(first, last, m_address.begin());
        }

        explicit mac(std::string_view addr) :
            m_address{  }
        {
            assert(addr.size() >= MAC_STR_SIZE);
            std::from_chars(std::next(addr.data(), 0), std::next(addr.data(), 2), m_address[0], 16);
            std::from_chars(std::next(addr.data(), 3), std::next(addr.data(), 5), m_address[1], 16);
            std::from_chars(std::next(addr.data(), 6), std::next(addr.data(), 8), m_address[2], 16);
            std::from_chars(std::next(addr.data(), 9), std::next(addr.data(), 11), m_address[3], 16);
            std::from_chars(std::next(addr.data(), 12), std::next(addr.data(), 14), m_address[4], 16);
            std::from_chars(std::next(addr.data(), 15), std::next(addr.data(), m_address.size()), m_address[5], 16);
        }

        mac()                       = default;

        mac(const mac&)             = default;

        mac(mac&&)                  = default;

        mac& operator=(const mac&)  = default;

        mac& operator=(mac&&)       = default;

        inline operator std::string() const noexcept
        {
            return fmt::format(
                "{:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}",
                m_address[0],
                m_address[1],
                m_address[2],
                m_address[3],
                m_address[4],
                m_address[5]
            );
        }

        inline operator uint8_t*() noexcept
        {
            return m_address.data();
        }

        inline operator const uint8_t*() const noexcept
        {
            return m_address.data();
        }

    private:

        std::array<uint8_t, MAC_SIZE> m_address;
    };
}

#endif