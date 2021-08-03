#include "utils/mac.hpp"

#include <charconv>

namespace hub::utils
{
    mac::mac(std::string_view addr) :
        m_address{  }
    {
        assert(addr.size() >= MAC_STR_SIZE);
        std::from_chars(std::next(addr.data(), 0),  std::next(addr.data(), 2),  m_address[0], 16);
        std::from_chars(std::next(addr.data(), 3),  std::next(addr.data(), 5),  m_address[1], 16);
        std::from_chars(std::next(addr.data(), 6),  std::next(addr.data(), 8),  m_address[2], 16);
        std::from_chars(std::next(addr.data(), 9),  std::next(addr.data(), 11), m_address[3], 16);
        std::from_chars(std::next(addr.data(), 12), std::next(addr.data(), 14), m_address[4], 16);
        std::from_chars(std::next(addr.data(), 15), std::next(addr.data(), 17), m_address[5], 16);
    }

    mac::operator std::string() const noexcept
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

    mac::operator uint8_t*() noexcept
    {
        return m_address.data();
    }

    mac::operator const uint8_t*() const noexcept
    {
        return m_address.data();
    }
}
