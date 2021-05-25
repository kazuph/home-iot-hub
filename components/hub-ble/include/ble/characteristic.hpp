#ifndef HUB_BLE_CHARACTERISTIC_H
#define HUB_BLE_CHARACTERISTIC_H

#include "event.hpp"

#include "esp_gatt_defs.h"

#include <cstdint>
#include <vector>
#include <memory>
#include <utility>

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
        characteristic(std::weak_ptr<client> client_ptr, std::pair<uint16_t, uint16_t> service_handle_range, esp_gattc_char_elem_t characteristic);

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
        uint16_t get_handle() const noexcept
        {
            return m_characteristic.char_handle;
        }

        /**
         * @brief Get characteristic uuid.
         * 
         * @return esp_bt_uuid_t 
         */
        esp_bt_uuid_t get_uuid() const noexcept
        {
            return m_characteristic.uuid;
        }

        /**
         * @brief Get all the descriptors for the given characteristic.
         * 
         * @return std::vector<descriptor> 
         */
        std::vector<descriptor> get_descriptors() const;

        /**
         * @brief Get the descriptor by by uuid.
         * 
         * @return descriptor 
         */
        descriptor get_descriptor_by_uuid(const esp_bt_uuid_t* uuid) const;

        /**
         * @brief Subscribe to characteristic notifications.
         */
        void subscribe(event::notify_function_t callback);

        /**
         * @brief Unsubscribe from characteristic notifications.
         */
        void unsubscribe();

    private:

        static constexpr const char* TAG{ "hub::ble::characteristic" };

        esp_gattc_char_elem_t           m_characteristic;
        std::pair<uint16_t, uint16_t>   m_service_handle_range;
        std::weak_ptr<client>           m_client_ptr;
    };
}

#endif