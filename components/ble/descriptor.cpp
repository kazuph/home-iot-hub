#include "descriptor.hpp"
#include "hub_error.h"

#include "esp_err.h"

namespace hub::ble
{
    descriptor::descriptor(std::shared_ptr<client> client_ptr, esp_gattc_descr_elem_t descriptor) :
        m_descriptor(descriptor),
        m_client_ptr(client_ptr)
    {

    }

    void descriptor::write(std::vector<uint8_t> data)
    {
        if (esp_err_t result = esp_ble_gattc_write_char_descr(
                m_client_ptr->m_gattc_interface, 
                m_client_ptr->m_connection_id,
                m_descriptor.char_handle,
                data.size(),
                data.data(),
                ESP_GATT_WRITE_TYPE_RSP,
                ESP_GATT_AUTH_REQ_NONE); 
            result != ESP_OK)
        {
            throw hub::esp_exception(ESP_FAIL, "Write descriptor failed.");
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
            throw hub::esp_exception(ESP_FAIL, "Write descriptor failed.");
        }
        else
        {
            throw hub::esp_exception(ESP_ERR_TIMEOUT, "Write descriptor timed out.");
        }
    }

    std::vector<uint8_t> descriptor::read() const
    {
        if (esp_err_t result = esp_ble_gattc_read_char_descr(
            m_client_ptr->m_gattc_interface, 
            m_client_ptr->m_connection_id,
            m_descriptor.char_handle,
            ESP_GATT_AUTH_REQ_NONE); 
            result != ESP_OK)
        {
            throw hub::esp_exception(ESP_FAIL, "Read descriptor failed.");
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
            throw hub::esp_exception(ESP_FAIL, "Read descriptor failed.");
        }
        else
        {
            throw hub::esp_exception(ESP_ERR_TIMEOUT, "Read descriptor timed out.");
        }

        return std::move(m_client_ptr->m_descriptor_data_cache);
    }
}