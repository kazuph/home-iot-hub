#ifndef HUB_BLE_DESCRIPTOR_HPP
#define HUB_BLE_DESCRIPTOR_HPP

#include <cstdint>
#include <vector>
#include <memory>

#include "esp_err.h"
#include "esp_gatt_defs.h"

#include "tl/expected.hpp"

namespace hub::ble
{
    class client;

    /**
     * @brief Class representing BLE descriptors.
     */
    class descriptor
    {
    public:

        descriptor() = delete;

        /**
         * @brief Construct the descriptor object. Constructor of the class should not be called directly.
         * Instead, characteristic objects should be obtained from the characteristic object.
         * 
         * @param client_ptr Shared pointer to the descriptor owner.
         * @param descriptor Descriptor object from ESP-IDF API.
         * 
         * @see characteristic
         */
        descriptor(std::weak_ptr<client> client_ptr, esp_gattc_descr_elem_t descriptor);

        /**
         * @brief Write data to the descriptor.
         * 
         * @param data 
         * @return tl::expected<void, esp_err_t> 
         */
        tl::expected<void, esp_err_t> write(std::vector<uint8_t> data) noexcept;

        /**
         * @brief Read data from the descriptor.
         * 
         * @return tl::expected<std::vector<uint8_t>, esp_err_t> 
         */
        tl::expected<std::vector<uint8_t>, esp_err_t> read() const noexcept;

        /**
         * @brief Get the descriptor handle.
         * 
         * @return uint16_t 
         */
        uint16_t get_handle() const
        {
            return m_descriptor.handle;
        }

    private:

        static constexpr const char* TAG{ "hub::ble::descriptor" };

        esp_gattc_descr_elem_t  m_descriptor;
        std::weak_ptr<client>   m_client_ptr;
    };
}

#endif