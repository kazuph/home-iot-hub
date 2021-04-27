#ifndef SERVICE_HPP
#define SERVICE_HPP

#include "esp_gatt_defs.h"

#include <vector>
#include <memory>

namespace hub::ble
{
    class client;
    class characteristic;
    
    /**
     * @brief Class service represents a single BLE service.
     */
    class service
    {
    public:

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
        service(std::shared_ptr<client> client_ptr, esp_gattc_service_elem_t service);

        /**
         * @brief Get all the characteristics for the given service.
         * 
         * @return std::vector<characteristic> 
         */
        std::vector<characteristic> get_characteristics() const;

    private:

        static constexpr const char* TAG{ "SERVICE" };

        esp_gattc_service_elem_t    m_service;
        std::shared_ptr<client>     m_client_ptr;
    };
}

#endif