#ifndef SERVICE_HPP
#define SERVICE_HPP

#include "esp_gatt_defs.h"

#include <vector>
#include <memory>

namespace hub::ble
{
    class client;
    class characteristic;
    
    class service
    {
    public:

        service() = delete;

        service(std::shared_ptr<client> client_ptr, esp_gattc_service_elem_t service);

        std::vector<characteristic> get_characteristics() const;

    private:

        static constexpr const char* TAG{ "SERVICE" };

        esp_gattc_service_elem_t    m_service;
        std::shared_ptr<client>     m_client_ptr;
    };
}

#endif