#ifndef HUB_BLE_SERVICE_HPP
#define HUB_BLE_SERVICE_HPP

#include <vector>
#include <memory>
#include <utility>

#include "esp_log.h"
#include "esp_gatt_defs.h"

#include "tl/expected.hpp"

namespace hub::ble
{
    class client;
    class characteristic;
    
    /**
     * @brief BLE service class.
     * 
     */
    class service
    {
    public:

        /**
         * @brief Construct a new service object.
         * 
         */
        service() = delete;

        /**
         * @brief Construct the service object. Constructor of the class should not be called directly.
         * Instead, service objects should be obtained from the client object.
         * 
         * @param client_ptr Shared pointer to the service owner.
         * @param service Service object from ESP-IDF API.
         * 
         * @see client
         */
        service(std::weak_ptr<client> client_ptr, esp_gattc_service_elem_t service);

        /**
         * @brief Check if service is a primary service.
         * 
         * @return bool 
         */
        bool is_primary() const noexcept
        {
            return m_service.is_primary;
        }

        /**
         * @brief Get a pair of handles: start service handler and end service handle.
         * 
         * @return std::pair<uint16_t, uint16_t> Pair of handles.
         */
        std::pair<uint16_t, uint16_t> get_handle_range() const noexcept
        {
            return std::make_pair(m_service.start_handle, m_service.end_handle);
        }

        /**
         * @brief Get the start service handle.
         * 
         * @return uint16_t service start handle
         */
        uint16_t get_start_handle() const noexcept
        {
            return m_service.start_handle;
        }

        /**
         * @brief Get the end service handle.
         * 
         * @return uint16_t service end handle
         */
        uint16_t get_end_handle() const noexcept
        {
            return m_service.end_handle;
        }

        /**
         * @brief Get service uuid.
         * 
         * @return esp_bt_uuid_t 
         */
        esp_bt_uuid_t get_uuid() const noexcept
        {
            return m_service.uuid;
        }

        /**
         * @brief Get the characteristics object
         * 
         * @return tl::expected<std::vector<characteristic>, esp_gatt_status_t> 
         */
        tl::expected<std::vector<characteristic>, esp_gatt_status_t> get_characteristics() const noexcept;

        /**
         * @brief Get characteristic by its uuid.
         * 
         * @param uuid 
         * @return tl::expected<characteristic, esp_gatt_status_t> 
         */
        tl::expected<characteristic, esp_gatt_status_t> get_characteristic_by_uuid(const esp_bt_uuid_t* uuid) const noexcept;

    private:

        static constexpr const char* TAG{ "hub::ble::service" };

        esp_gattc_service_elem_t    m_service;
        std::weak_ptr<client>       m_client_ptr;
    };
}

#endif