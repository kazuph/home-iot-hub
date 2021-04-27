#include "characteristic.hpp"
#include "client.hpp"
#include "descriptor.hpp"
#include "error.hpp"

#include "esp_bt_defs.h"
#include "esp_gattc_api.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

namespace hub::ble
{
    characteristic::characteristic(std::shared_ptr<client> client_ptr, esp_gattc_char_elem_t characteristic) :
        m_characteristic(characteristic),
        m_client_ptr(client_ptr)
    {

    }

    void characteristic::write(std::vector<uint8_t> data)
    {
        if (esp_err_t result = esp_ble_gattc_write_char(
                m_client_ptr->m_gattc_interface, 
                m_client_ptr->m_connection_id,
                m_characteristic.char_handle,
                data.size(),
                data.data(),
                ESP_GATT_WRITE_TYPE_RSP,
                ESP_GATT_AUTH_REQ_NONE);
            result != ESP_OK)
        {
            throw hub::esp_exception(ESP_FAIL, "Write characteristic failed.");
        }

        EventBits_t bits = xEventGroupWaitBits(
            m_client_ptr->m_event_group, 
            client::WRITE_CHAR_BIT | client::FAIL_BIT, 
            pdTRUE, 
            pdFALSE, 
            static_cast<TickType_t>(BLE_TIMEOUT));

        if (bits & client::WRITE_CHAR_BIT)
        {
            ESP_LOGI(TAG, "Write characteristic success.");
        }
        else if (bits & client::FAIL_BIT)
        {
            throw hub::esp_exception(ESP_FAIL, "Write characteristic failed.");
        }
        else
        {
            throw hub::esp_exception(ESP_ERR_TIMEOUT, "Write characteristic timed out.");
        }
    }

    std::vector<uint8_t> characteristic::read() const
    {
        if (esp_err_t result = esp_ble_gattc_read_char(
                m_client_ptr->m_gattc_interface, 
                m_client_ptr->m_connection_id,
                m_characteristic.char_handle,
                ESP_GATT_AUTH_REQ_NONE); 
            result != ESP_OK)
        {
            throw hub::esp_exception(ESP_FAIL, "Read characteristic failed.");
        }

        EventBits_t bits = xEventGroupWaitBits(
            m_client_ptr->m_event_group, 
            client::READ_CHAR_BIT | client::FAIL_BIT, 
            pdTRUE, 
            pdFALSE, 
            static_cast<TickType_t>(BLE_TIMEOUT));

        if (bits & client::READ_CHAR_BIT)
        {
            ESP_LOGI(TAG, "Read characteristic success.");
        }
        else if (bits & client::FAIL_BIT)
        {
            throw hub::esp_exception(ESP_FAIL, "Read characteristic failed.");
        }
        else
        {
            throw hub::esp_exception(ESP_ERR_TIMEOUT, "Read characteristic timed out.");
        }

        return std::move(m_client_ptr->m_characteristic_data_cache);
    }

    uint16_t characteristic::get_handle() const noexcept
    {
        return m_characteristic.char_handle;
    }

    std::vector<descriptor> characteristic::get_descriptors() const
    {
        return std::vector<descriptor>();
    }
}