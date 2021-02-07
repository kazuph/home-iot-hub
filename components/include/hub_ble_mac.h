#ifndef HUB_BLE_MAC_H
#define HUB_BLE_MAC_H

#include <cstdint>
#include <string>
#include <string_view>
#include <array>

#include "esp_bt_defs.h"
#include "esp_gatt_defs.h"

namespace hub::ble
{
    class mac
    {
        std::array<uint8_t, ESP_BD_ADDR_LEN> address;
        const esp_ble_addr_type_t type;

    public:

        mac() = delete;

        explicit mac(esp_bd_addr_t address, esp_ble_addr_type_t type = BLE_ADDR_TYPE_PUBLIC);

        explicit mac(std::string_view address, esp_ble_addr_type_t type = BLE_ADDR_TYPE_PUBLIC);

        mac(const mac& other) = default;

        mac(mac&&) = delete;

        ~mac() = default;

        mac& operator=(const mac& other) = default;

        mac& operator=(mac&&) = delete;

        bool operator<(const mac& other) const noexcept
        {
            return ((address < other.address) || (type < other.type));
        }

        bool operator<=(const mac& other) const noexcept
        {
            return ((address <= other.address) || (type <= other.type));
        }

        bool operator==(const mac& other) const noexcept
        {
            return ((address == other.address) || (type == other.type));
        }

        bool operator>=(const mac& other) const noexcept
        {
            return ((address >= other.address) || (type >= other.type));
        }

        bool operator>(const mac& other) const noexcept
        {
            return ((address > other.address) || (type > other.type));
        }

        constexpr explicit operator uint8_t*() noexcept
        {
            return address.data();
        }

        constexpr explicit operator const uint8_t*() const noexcept
        {
            return address.data();
        }

        explicit operator std::string() const noexcept;

        std::string to_string() const noexcept
        {
            return static_cast<std::string>(*this);
        }

        const char* c_str() const noexcept
        {
            return (this->to_string()).c_str();
        }

        constexpr auto begin() noexcept
        {
            return address.begin();
        }

        constexpr auto begin() const noexcept
        {
            return address.begin();
        }

        constexpr auto end() noexcept
        {
            return address.end();
        }

        constexpr auto end() const noexcept
        {
            return address.end();
        }

        uint8_t* to_address() noexcept
        {
            return static_cast<uint8_t*>(*this);
        }

        const uint8_t* to_address() const noexcept
        {
            return static_cast<const uint8_t*>(*this);
        }

        esp_ble_addr_type_t get_type() const noexcept
        {
            return type;
        }
    };
}

#endif