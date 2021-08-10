#include "ble/service.hpp"

#include <stdexcept>

#include "esp_bt_defs.h"
#include "esp_gattc_api.h"

#include "ble/client.hpp"
#include "ble/characteristic.hpp"

namespace hub::ble
{
    service::service(std::weak_ptr<client> client_ptr, esp_gattc_service_elem_t service) :
        m_service       { service },
        m_client_ptr    { client_ptr }
    {
        
    }

    tl::expected<std::vector<characteristic>, esp_gatt_status_t> service::get_characteristics() const noexcept
    {
        esp_gatt_status_t result = ESP_GATT_OK;
        uint16_t characteristic_count = 0U;
        std::vector<esp_gattc_char_elem_t> characteristics;

        auto shared_client = m_client_ptr.lock();

        if (!shared_client)
        {
            ESP_LOGE(TAG, "Client is not connected.");
            return tl::expected<std::vector<characteristic>, esp_gatt_status_t>(tl::unexpect, ESP_GATT_ERROR);
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
            ESP_LOGE(TAG, "Could not get characteristics count.");
            return tl::expected<std::vector<characteristic>, esp_gatt_status_t>(tl::unexpect, result);
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
            ESP_LOGE(TAG, "Could not retrieve characteristics.");
            return tl::expected<std::vector<characteristic>, esp_gatt_status_t>(tl::unexpect, result);
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
    
    tl::expected<characteristic, esp_gatt_status_t> service::get_characteristic_by_uuid(const esp_bt_uuid_t* uuid) const noexcept
    {
        esp_gatt_status_t result = ESP_GATT_OK;
        uint16_t characteristic_count = 0U;
        std::vector<esp_gattc_char_elem_t> characteristics;

        auto shared_client = m_client_ptr.lock();

        if (!shared_client)
        {
            ESP_LOGE(TAG, "Client is not connected.");
            return tl::expected<characteristic, esp_gatt_status_t>(tl::unexpect, ESP_GATT_ERROR);
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
            ESP_LOGE(TAG, "Could not get characteristics count.");
            return tl::expected<characteristic, esp_gatt_status_t>(tl::unexpect, result);
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
            ESP_LOGE(TAG, "Could not retrieve characteristics.");
            return tl::expected<characteristic, esp_gatt_status_t>(tl::unexpect, result);
        }

        return characteristic(m_client_ptr, get_handle_range(), characteristics.front());
    }
}