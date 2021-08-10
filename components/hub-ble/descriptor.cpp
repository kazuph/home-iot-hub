#include "ble/descriptor.hpp"

#include <stdexcept>

#include "esp_log.h"
#include "esp_bt_defs.h"
#include "esp_gattc_api.h"

#include "ble/client.hpp"

namespace hub::ble
{
    descriptor::descriptor(std::weak_ptr<client> client_ptr, esp_gattc_descr_elem_t descriptor) :
        m_descriptor(descriptor),
        m_client_ptr(client_ptr)
    {
        
    }

    tl::expected<void, esp_err_t> descriptor::write(std::vector<uint8_t> data) noexcept
    {
        auto shared_client = m_client_ptr.lock();

        if (!shared_client)
        {
            ESP_LOGE(TAG, "Client is not connected.");
            return tl::expected<void, esp_err_t>(tl::unexpect, ESP_ERR_INVALID_STATE);
        }

        if (esp_err_t result = esp_ble_gattc_write_char_descr(
                shared_client->m_gattc_interface, 
                shared_client->m_connection_id,
                m_descriptor.handle,
                data.size(),
                data.data(),
                ESP_GATT_WRITE_TYPE_RSP,
                ESP_GATT_AUTH_REQ_NONE); 
            result != ESP_OK)
        {
            ESP_LOGE(TAG, "Write descriptor failed.");
            return tl::expected<void, esp_err_t>(tl::unexpect, result);
        }

        EventBits_t bits = xEventGroupWaitBits(
            shared_client->m_event_group, 
            client::WRITE_DESCR_BIT | client::FAIL_BIT, 
            pdTRUE, 
            pdFALSE, 
            static_cast<TickType_t>(BLE_TIMEOUT));

        if (bits & client::WRITE_DESCR_BIT)
        {
            ESP_LOGI(TAG, "Write descriptor success.");
            return tl::expected<void, esp_err_t>();
        }
        else if (bits & client::FAIL_BIT)
        {
            ESP_LOGE(TAG, "Write descriptor failed.");
            return tl::expected<void, esp_err_t>(tl::unexpect, ESP_FAIL);
        }
        else
        {
            ESP_LOGE(TAG, "Write descriptor timeout.");
            return tl::expected<void, esp_err_t>(tl::unexpect, ESP_ERR_TIMEOUT);
        }
    }

    tl::expected<std::vector<uint8_t>, esp_err_t> descriptor::read() const noexcept
    {
        auto shared_client = m_client_ptr.lock();

        if (!shared_client)
        {
            ESP_LOGE(TAG, "Client is not connected.");
            return tl::make_unexpected<esp_err_t>(ESP_ERR_INVALID_STATE);
        }

        if (esp_err_t result = esp_ble_gattc_read_char_descr(
            shared_client->m_gattc_interface, 
            shared_client->m_connection_id,
            m_descriptor.handle,
            ESP_GATT_AUTH_REQ_NONE); 
            result != ESP_OK)
        {
            ESP_LOGE(TAG, "Read descriptor failed.");
            return tl::expected<std::vector<uint8_t>, esp_err_t>(tl::unexpect, result);
        }

        EventBits_t bits = xEventGroupWaitBits(
            shared_client->m_event_group, 
            client::READ_DESCR_BIT | client::FAIL_BIT, 
            pdTRUE, 
            pdFALSE, 
            static_cast<TickType_t>(BLE_TIMEOUT));

        if (bits & client::READ_DESCR_BIT)
        {
            ESP_LOGI(TAG, "Read descriptor success.");
            return std::vector<uint8_t>(std::move(shared_client->m_descriptor_data_cache));
        }
        else if (bits & client::FAIL_BIT)
        {
            ESP_LOGE(TAG, "Read descriptor failed.");
            return tl::expected<std::vector<uint8_t>, esp_err_t>(tl::unexpect, ESP_FAIL);
        }
        else
        {
            ESP_LOGE(TAG, "Read descriptor timeout.");
            return tl::expected<std::vector<uint8_t>, esp_err_t>(tl::unexpect, ESP_ERR_TIMEOUT);
        }
    }
}