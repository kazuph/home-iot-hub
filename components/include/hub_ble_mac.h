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
    /**
     * @brief Class for BLE MAC address representation. It offers conversions to std::string and uint8_t*.
     */
    class mac
    {
    public:

        mac()
            m_address   { 0, 0, 0, 0, 0, 0 },
            m_type      { 0 } 
        {

        }

        explicit mac(esp_bd_addr_t m_address, esp_ble_addr_type_t m_type = BLE_ADDR_TYPE_PUBLIC);

        explicit mac(std::string_view m_address, esp_ble_addr_type_t m_type = BLE_ADDR_TYPE_PUBLIC);

        mac(const mac& other)           = default;

        mac(mac&&)                      = default;

        ~mac()                          = default;

        mac& operator=(const mac&)      = default; 

        mac& operator=(mac&&)           = default;

        bool operator==(const mac& other) const noexcept
        {
            return ((m_address == other.m_address) && (m_type == other.m_type));
        }

        bool operator!=(const mac& other) const noexcept
        {
            return !operator==(other);
        }

        /**
         * @brief Conversion to uint8_t*, implicintly convertible to esp_bd_addr_t.
         * 
         * @return uint8_t* Pointer to the first byte of the address.
         */
        constexpr explicit operator uint8_t*() noexcept
        {
            return m_address.data();
        }

        /**
         * @brief Conversion to uint8_t*, implicintly convertible to esp_bd_addr_t.
         * 
         * @return uint8_t* Pointer to the first byte of the address.
         */
        constexpr explicit operator const uint8_t*() const noexcept
        {
            return m_address.data();
        }

        /**
         * @brief Conversion to std:string. Converts address from byte form to XX:XX:XX:XX:XX:XX form.
         * 
         * @return std::string 
         */
        explicit operator std::string() const noexcept;

        /**
         * @brief Conversion to std:string. Converts address from byte form to XX:XX:XX:XX:XX:XX form.
         * Effectively calls operator std::string().
         * 
         * @return std::string 
         */
        std::string to_string() const noexcept
        {
            return static_cast<std::string>(*this);
        }
        
        constexpr auto begin() noexcept
        {
            return m_address.begin();
        }

        constexpr auto begin() const noexcept
        {
            return m_address.cbegin();
        }

        constexpr auto cbegin() const noexcept
        {
            return m_address.cbegin();
        }

        constexpr auto end() noexcept
        {
            return m_address.end();
        }

        constexpr auto end() const noexcept
        {
            return m_address.cend();
        }

        constexpr auto cend() const noexcept
        {
            return m_address.cend();
        }

        /**
         * @brief Conversion to uint8_t*, implicintly convertible to esp_bd_addr_t.
         * 
         * @return uint8_t* Pointer to the first byte of the address.
         */
        uint8_t* to_address() noexcept
        {
            return static_cast<uint8_t*>(*this);
        }

        /**
         * @brief Conversion to uint8_t*, implicintly convertible to esp_bd_addr_t.
         * 
         * @return uint8_t* Pointer to the first byte of the address.
         */
        const uint8_t* to_address() const noexcept
        {
            return static_cast<const uint8_t*>(*this);
        }

        /**
         * @brief Get address type.
         * 
         * @return esp_ble_addr_type_t Address type.
         */
        esp_ble_addr_type_t get_type() const noexcept
        {
            return m_type;
        }

    private:

        std::array<uint8_t, ESP_BD_ADDR_LEN>    m_address;
        esp_ble_addr_type_t                     m_type;
    };
}

#endif