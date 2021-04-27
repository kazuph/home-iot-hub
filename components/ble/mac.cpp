#include "mac.hpp"

#include <algorithm>
#include <charconv>

namespace hub::ble
{
    mac::mac(esp_bd_addr_t m_address, esp_ble_addr_type_t m_type) :
        m_address{ m_address[0], m_address[1], m_address[2], m_address[3], m_address[4], m_address[5] },
        m_type{ m_type } 
    {

    }

    mac::mac(std::string_view m_address, esp_ble_addr_type_t m_type) : 
        m_address{ 0, 0, 0, 0, 0, 0 }, 
        m_type{ m_type }
    {
        std::from_chars(std::next(m_address.data(), 0), std::next(m_address.data(), 2), this->m_address[0], 16);
        std::from_chars(std::next(m_address.data(), 3), std::next(m_address.data(), 5), this->m_address[1], 16);
        std::from_chars(std::next(m_address.data(), 6), std::next(m_address.data(), 8), this->m_address[2], 16);
        std::from_chars(std::next(m_address.data(), 9), std::next(m_address.data(), 11), this->m_address[3], 16);
        std::from_chars(std::next(m_address.data(), 12), std::next(m_address.data(), 14), this->m_address[4], 16);
        std::from_chars(std::next(m_address.data(), 15), std::next(m_address.data(), m_address.length()), this->m_address[5], 16);
    }

    mac::operator std::string() const noexcept
    {
        using namespace std::literals;

        std::string result{ "00:00:00:00:00:00"s };
        ptrdiff_t offs = 0;

        offs = static_cast<ptrdiff_t>(!((m_address[0]) >> 4));
        std::to_chars(std::next(result.data(), offs), std::next(result.data(), 2), m_address[0], 16);
        offs = static_cast<ptrdiff_t>(!((m_address[1]) >> 4));
        std::to_chars(std::next(result.data(), 3 + offs), std::next(result.data(), 5), m_address[1], 16);
        offs = static_cast<ptrdiff_t>(!((m_address[2]) >> 4));
        std::to_chars(std::next(result.data(), 6 + offs), std::next(result.data(), 8), m_address[2], 16);
        offs = static_cast<ptrdiff_t>(!((m_address[3]) >> 4));
        std::to_chars(std::next(result.data(), 9 + offs), std::next(result.data(), 11), m_address[3], 16);
        offs = static_cast<ptrdiff_t>(!((m_address[4]) >> 4));
        std::to_chars(std::next(result.data(), 12 + offs), std::next(result.data(), 14), m_address[4], 16);
        offs = static_cast<ptrdiff_t>(!((m_address[5]) >> 4));
        std::to_chars(std::next(result.data(), 15 + offs), std::next(result.data(), result.length()), m_address[5], 16);

        return result;
    }
}