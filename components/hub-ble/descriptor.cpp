#include "ble/descriptor.hpp"
#include "ble/client.hpp"

#include "esp_bt_defs.h"
#include "esp_gattc_api.h"
#include "esp_err.h"
#include "esp_log.h"

#include <stdexcept>

namespace hub::ble
{
    descriptor::descriptor(std::shared_ptr<client> client_ptr, esp_gattc_descr_elem_t descriptor) :
        m_descriptor(descriptor),
        m_client_ptr(client_ptr)
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
    }

    void descriptor::write(std::vector<uint8_t> data)
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        if (esp_err_t result = esp_ble_gattc_write_char_descr(
                m_client_ptr->m_gattc_interface, 
                m_client_ptr->m_connection_id,
                m_descriptor.handle,
                data.size(),
                data.data(),
                ESP_GATT_WRITE_TYPE_RSP,
                ESP_GATT_AUTH_REQ_NONE); 
            result != ESP_OK)
        {
            throw std::runtime_error("Write descriptor failed.");
        }

        EventBits_t bits = xEventGroupWaitBits(
            m_client_ptr->m_event_group, 
            client::WRITE_DESCR_BIT | client::FAIL_BIT, 
            pdTRUE, 
            pdFALSE, 
            static_cast<TickType_t>(BLE_TIMEOUT));

        if (bits & client::WRITE_DESCR_BIT)
        {
            ESP_LOGI(TAG, "Write descriptor success.");
        }
        else if (bits & client::FAIL_BIT)
        {
            throw std::runtime_error("Write descriptor failed.");
        }
        else
        {
            throw std::runtime_error("Write descriptor timed out.");
        }
    }

    std::vector<uint8_t> descriptor::read() const
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        
        if (esp_err_t result = esp_ble_gattc_read_char_descr(
            m_client_ptr->m_gattc_interface, 
            m_client_ptr->m_connection_id,
            m_descriptor.handle,
            ESP_GATT_AUTH_REQ_NONE); 
            result != ESP_OK)
        {
            throw std::runtime_error("Read descriptor failed.");
        }

        EventBits_t bits = xEventGroupWaitBits(
            m_client_ptr->m_event_group, 
            client::READ_DESCR_BIT | client::FAIL_BIT, 
            pdTRUE, 
            pdFALSE, 
            static_cast<TickType_t>(BLE_TIMEOUT));

        if (bits & client::READ_DESCR_BIT)
        {
            ESP_LOGI(TAG, "Read descriptor success.");
        }
        else if (bits & client::FAIL_BIT)
        {
            throw std::runtime_error("Read descriptor failed.");
        }
        else
        {
            throw std::runtime_error("Read descriptor timed out.");
        }

        return std::move(m_client_ptr->m_descriptor_data_cache);
    }
}