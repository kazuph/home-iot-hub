#include "ble/service.hpp"
#include "ble/client.hpp"
#include "ble/characteristic.hpp"

#include <stdexcept>

#include "esp_bt_defs.h"
#include "esp_gattc_api.h"

namespace hub::ble
{
    service::service(std::weak_ptr<client> client_ptr, esp_gattc_service_elem_t service) :
        m_service       { service },
        m_client_ptr    { client_ptr }
    {
        
    }

    std::vector<characteristic> service::get_characteristics() const
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        
        esp_gatt_status_t result = ESP_GATT_OK;
        uint16_t characteristic_count = 0U;
        std::vector<esp_gattc_char_elem_t> characteristics;

        auto shared_client = m_client_ptr.lock();

        if (!shared_client)
        {
            throw std::runtime_error("Client is not connected..");
        }

        result = esp_ble_gattc_get_attr_count(
            shared_client->m_gattc_interface, 
            shared_client->m_connection_id,
            ESP_GATT_DB_CHARACTERISTIC,
            m_service.start_handle,
            m_service.end_handle,
            ESP_GATT_ILLEGAL_HANDLE,
            &characteristic_count);

        if (result != ESP_GATT_OK)
        {
            throw std::runtime_error("Could not get characteristics count.");
        }

        characteristics.resize(characteristic_count);

        result = esp_ble_gattc_get_all_char(
            shared_client->m_gattc_interface, 
            shared_client->m_connection_id,
            m_service.start_handle,
            m_service.end_handle,
            characteristics.data(),
            &characteristic_count,
            0);

        if (result != ESP_GATT_OK)
        {
            throw std::runtime_error("Could not retrieve characteristics.");
        }

        {
            std::vector<characteristic> result;
            result.reserve(characteristics.size());
            std::transform(characteristics.cbegin(), characteristics.cend(), std::back_inserter(result), [this](auto elem) { 
                return characteristic(m_client_ptr, get_handle_range(), elem); 
            });
            return result;
        }
    }
    
    characteristic service::get_characteristic_by_uuid(const esp_bt_uuid_t* uuid) const
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        esp_gatt_status_t result = ESP_GATT_OK;
        uint16_t characteristic_count = 0U;
        std::vector<esp_gattc_char_elem_t> characteristics;

        auto shared_client = m_client_ptr.lock();

        if (!shared_client)
        {
            throw std::runtime_error("Client is not connected..");
        }

        result = esp_ble_gattc_get_attr_count(
            shared_client->m_gattc_interface, 
            shared_client->m_connection_id,
            ESP_GATT_DB_CHARACTERISTIC,
            m_service.start_handle,
            m_service.end_handle,
            ESP_GATT_ILLEGAL_HANDLE,
            &characteristic_count);

        if (result != ESP_GATT_OK)
        {
            throw std::runtime_error("Could not get characteristics count.");
        }

        characteristics.resize(characteristic_count);

        result = esp_ble_gattc_get_char_by_uuid(
            shared_client->m_gattc_interface, 
            shared_client->m_connection_id,
            m_service.start_handle,
            m_service.end_handle,
            *uuid,
            characteristics.data(),
            &characteristic_count);

        if (result != ESP_GATT_OK)
        {
            throw std::runtime_error("Could not retrieve characteristics.");
        }

        return characteristic(m_client_ptr, get_handle_range(), characteristics.front());
    }
}