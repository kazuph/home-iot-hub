#include "hub_ble_mac.h"
#include "esp_log.h"

#include <algorithm>
#include <charconv>

namespace hub::ble
{
    static constexpr const char* TAG{ "MAC" };

    mac::mac(esp_bd_addr_t address, esp_ble_addr_type_t type) :
        address{ address[0], address[1], address[2], address[3], address[4], address[5] },
        type{ type } 
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
    }

    mac::mac(std::string_view address, esp_ble_addr_type_t type) : address{ 0, 0, 0, 0, 0, 0 }, type{ type }
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        std::from_chars(std::next(address.data(), 0), std::next(address.data(), 2), this->address[0], 16);
        std::from_chars(std::next(address.data(), 3), std::next(address.data(), 5), this->address[1], 16);
        std::from_chars(std::next(address.data(), 6), std::next(address.data(), 8), this->address[2], 16);
        std::from_chars(std::next(address.data(), 9), std::next(address.data(), 11), this->address[3], 16);
        std::from_chars(std::next(address.data(), 12), std::next(address.data(), 14), this->address[4], 16);
        std::from_chars(std::next(address.data(), 15), std::next(address.data(), address.length()), this->address[5], 16);
    }

    mac::operator std::string() const noexcept
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        using namespace std::literals;

        std::string result{ "00:00:00:00:00:00"s };
        ptrdiff_t offs = 0;

        offs = static_cast<ptrdiff_t>(!((address[0]) >> 4));
        std::to_chars(std::next(result.data(), offs), std::next(result.data(), 2), address[0], 16);
        offs = static_cast<ptrdiff_t>(!((address[1]) >> 4));
        std::to_chars(std::next(result.data(), 3 + offs), std::next(result.data(), 5), address[1], 16);
        offs = static_cast<ptrdiff_t>(!((address[2]) >> 4));
        std::to_chars(std::next(result.data(), 6 + offs), std::next(result.data(), 8), address[2], 16);
        offs = static_cast<ptrdiff_t>(!((address[3]) >> 4));
        std::to_chars(std::next(result.data(), 9 + offs), std::next(result.data(), 11), address[3], 16);
        offs = static_cast<ptrdiff_t>(!((address[4]) >> 4));
        std::to_chars(std::next(result.data(), 12 + offs), std::next(result.data(), 14), address[4], 16);
        offs = static_cast<ptrdiff_t>(!((address[5]) >> 4));
        std::to_chars(std::next(result.data(), 15 + offs), std::next(result.data(), result.length()), address[5], 16);

        return result;
    }
}