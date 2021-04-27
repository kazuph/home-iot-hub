#ifndef DESCRIPTOR_HPP
#define DESCRIPTOR_HPP

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
        descriptor(std::shared_ptr<client> client_ptr, esp_gattc_descr_elem_t descriptor);

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

    private:

        static constexpr const char* TAG{ "DESCRIPTOR" };

        esp_gattc_descr_elem_t  m_descriptor;
        std::shared_ptr<client> m_client_ptr;
    };
}

#endif