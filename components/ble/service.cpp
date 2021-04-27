#include "service.hpp"
#include "hub_error.h"

#include "range/v3/view/view.hpp"

namespace hub::ble
{
    service::service(std::shared_ptr<client> client_ptr, esp_gattc_service_elem_t service) :
        m_service{ service }
        m_client_ptr{ client_ptr }
    {
        
    }

    std::vector<characteristic> service::get_characteristics() const
    {
        using namespace ranges;
        using namespace ranges::views;

        esp_gatt_status_t result = ESP_GATT_OK;
        uint16_t characteristic_count = 0U;
        std::vector<esp_gattc_char_elem_t> characteristics;

        result = esp_ble_gattc_get_attr_count(
            m_client_ptr->m_gattc_interface, 
            m_client_ptr->m_connection_id,
            ESP_GATT_DB_CHARACTERISTIC,
            m_service.service_start_handle,
            m_service.service_end_handle,
            ESP_GATT_ILLEGAL_HANDLE,
            &characteristic_count);

        if (result != ESP_GATT_OK)
        {
            throw hub::esp_exception(ESP_FAIL, "Could not get characteristics count.");
        }

        characteristic_count.resize(characteristics);

        result = esp_ble_gattc_get_all_char(
            m_client_ptr->m_gattc_interface, 
            m_client_ptr->m_connection_id,
            m_service.service_start_handle,
            m_service.service_end_handle,
            characteristics.data(),
            characteristics.size(),
            0);

        if (result != ESP_GATT_OK)
        {
            throw hub::esp_exception(ESP_FAIL, "Could not retrieve characteristics.");
        }

        return 
            characteristics                                                                         | 
            transform([m_client_ptr](auto elem) { return characteristic(m_client_ptr, elem) }))     | 
            to<std::vector<characteristic>>();
    }
}