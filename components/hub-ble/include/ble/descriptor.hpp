#ifndef HUB_BLE_DESCRIPTOR_HPP
#define HUB_BLE_DESCRIPTOR_HPP

#include "esp_gatt_defs.h"

#include <cstdint>
#include <vector>
#include <memory>

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
         */
        void write(std::vector<uint8_t> data);

        /**
         * @brief Read data from the descriptor.
         * 
         * @return std::vector<uint8_t> 
         */
        std::vector<uint8_t> read() const;

        uint16_t get_handle() const
        {
            return m_descriptor.handle;
        }

    private:

        static constexpr const char* TAG{ "DESCRIPTOR" };

        esp_gattc_descr_elem_t  m_descriptor;
        std::weak_ptr<client> m_client_ptr;
    };
}

#endif