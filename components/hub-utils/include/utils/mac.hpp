#ifndef HUB_UTILS_MAC_HPP
#define HUB_UTILS_MAC_HPP

#include <cassert>
#include <array>
#include <algorithm>
#include <cstdint>
#include <string>
#include <charconv>

#include "fmt/core.h"

namespace hub::utils
{
    class mac
    {
    public:

        explicit mac(std::initializer_list<uint8_t> init) :
            m_address{  }
        {
            std::copy(init.begin(), init.end(), m_address.begin());
        }

        template<typename InIt>
        explicit mac(InIt first, InIt last) :
            m_address{  }
        {
            std::copy(first, last, m_address.begin());
        }

        explicit mac(std::string_view addr) :
            m_address{  }
        {
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
                m_address[5],
                m_address[6]
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

        std::array<uint8_t, 6> m_address;
    };
}

#endif