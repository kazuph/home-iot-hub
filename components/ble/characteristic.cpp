#include "characteristic.hpp"
#include "client.hpp"
#include "descriptor.hpp"
#include "error.hpp"
#include "timing.hpp"

#include "esp_err.h"
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
        esp_gatt_status_t result = ESP_GATT_OK;
        uint16_t descriptor_count = 0U;
        std::vector<esp_gattc_descr_elem_t> descriptors;

        result = esp_ble_gattc_get_attr_count(
            m_client_ptr->m_gattc_interface, 
            m_client_ptr->m_connection_id,
            ESP_GATT_DB_DESCRIPTOR,
            0,
            0,
            m_characteristic.char_handle,
            &descriptor_count);

        if (result != ESP_GATT_OK)
        {
            throw hub::esp_exception(ESP_FAIL, "Could not get desriptor count.");
        }

        descriptors.resize(descriptor_count);

        result = esp_ble_gattc_get_all_descr(
            m_client_ptr->m_gattc_interface, 
            m_client_ptr->m_connection_id,
            m_characteristic.char_handle,
            descriptors.data(),
            &descriptor_count,
            0);

        if (result != ESP_GATT_OK)
        {
            throw hub::esp_exception(ESP_FAIL, "Could not retrieve desriptors.");
        }

        {
            std::vector<descriptor> result;
            result.reserve(descriptors.size());
            std::transform(descriptors.cbegin(), descriptors.cend(), std::back_inserter(result), [this](auto elem) { return descriptor(m_client_ptr, elem); });
            return result;
        }
    }

    void characteristic::subscribe()
    {
        if (esp_err_t result = esp_ble_gattc_register_for_notify(
                m_client_ptr->m_gattc_interface, 
                m_client_ptr->m_address.to_address(), 
                m_characteristic.char_handle); 
            result != ESP_OK)
        {
            throw hub::esp_exception(ESP_FAIL, "Subscribe to characteristic failed.");
        }

        EventBits_t bits = xEventGroupWaitBits(
            m_client_ptr->m_event_group, 
            client::REG_FOR_NOTIFY_BIT | client::FAIL_BIT, 
            pdTRUE, 
            pdFALSE, 
            static_cast<TickType_t>(BLE_TIMEOUT));

        if (bits & client::REG_FOR_NOTIFY_BIT)
        {
            ESP_LOGI(TAG, "Subscribe to characteristic success.");
        }
        else if (bits & client::FAIL_BIT)
        {
            throw hub::esp_exception(ESP_FAIL, "Subscribe to characteristic failed.");
        }
        else
        {
            throw hub::esp_exception(ESP_ERR_TIMEOUT, "Subscribe to characteristic timed out.");
        }
    }

    void characteristic::unsubscribe()
    {
        if (esp_err_t result = esp_ble_gattc_unregister_for_notify(
                m_client_ptr->m_gattc_interface, 
                m_client_ptr->m_address.to_address(), 
                m_characteristic.char_handle); 
            result != ESP_OK)
        {
            throw hub::esp_exception(ESP_FAIL, "Unsubscribe from characteristic failed.");
        }

        EventBits_t bits = xEventGroupWaitBits(
            m_client_ptr->m_event_group, 
            client::UNREG_FOR_NOTIFY_BIT | client::FAIL_BIT, 
            pdTRUE, 
            pdFALSE, 
            static_cast<TickType_t>(BLE_TIMEOUT));

        if (bits & client::UNREG_FOR_NOTIFY_BIT)
        {
            ESP_LOGI(TAG, "Unsubscribe from characteristic success.");
        }
        else if (bits & client::FAIL_BIT)
        {
            throw hub::esp_exception(ESP_FAIL, "Unsubscribe from characteristic failed.");
        }
        else
        {
            throw hub::esp_exception(ESP_ERR_TIMEOUT, "Unsubscribe from characteristic timed out.");
        }
    }
}