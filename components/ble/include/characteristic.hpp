#ifndef CHARACTERISTIC_H
#define CHARACTERISTIC_H

#include "esp_gatt_defs.h"

#include <cstdint>
#include <vector>
#include <memory>

namespace hub::ble
{
    class client;
    class descriptor;
    
    /**
     * @brief Class representing BLE characteristics.
     */
    class characteristic
    {
    public:

        characteristic() = delete;

        /**
         * @brief Construct the characteristic object. Constructor of the class should not be called directly.
         * Instead, characteristic objects should be obtained from the service object.
         * 
         * @param client_ptr Shared pointer to the characteristic owner.
         * @param characteristic Characteristic object from ESP-IDF API.
         * 
         * @see service
         */
        characteristic(std::shared_ptr<client> client_ptr, esp_gattc_char_elem_t characteristic);

        /**
         * @brief Write data to the characteristic.
         * 
         * @param data 
         */
        void write(std::vector<uint8_t> data);

        /**
         * @brief Read data from the characteristic.
         * 
         * @return std::vector<uint8_t> 
         */
        std::vector<uint8_t> read() const;

        /**
         * @brief Get the characteristic handle.
         * 
         * @return uint16_t 
         */
        uint16_t get_handle() const noexcept;

        /**
         * @brief Get all the descriptors for the given characteristic.
         * 
         * @return std::vector<descriptor> 
         */
        std::vector<descriptor> get_descriptors() const;

        /**
         * @brief Subscribe to characteristic notifications.
         */
        void subscribe();

        /**
         * @brief Unsubscribe from characteristic notifications.
         */
        void unsubscribe();

    private:

        static constexpr const char* TAG{ "CHARACTERISTIC" };

        esp_gattc_char_elem_t   m_characteristic;
        std::shared_ptr<client> m_client_ptr;
    };
}

#endif