#include "ble/characteristic.hpp"
#include "ble/client.hpp"
#include "ble/descriptor.hpp"
#include "timing/timing.hpp"

#include "esp_log.h"
#include "esp_err.h"
#include "esp_bt_defs.h"
#include "esp_gattc_api.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include <stdexcept>

namespace hub::ble
{
    characteristic::characteristic(std::weak_ptr<client> client_ptr, std::pair<uint16_t, uint16_t> service_handle_range, esp_gattc_char_elem_t characteristic) :
        m_characteristic(characteristic),
        m_service_handle_range(service_handle_range),
        m_client_ptr(client_ptr)
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
    }

    result<void> characteristic::write(std::vector<uint8_t> data) noexcept
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        using result_type = result<void>;

        auto shared_client = m_client_ptr.lock();

        if (!shared_client)
        {
            ESP_LOGE(TAG, "Client is not connected.");
            return result_type::failure(errc::not_connected);
        }

        {
            async::lock lock{ shared_client->m_mutex };

            if (esp_err_t result = esp_ble_gattc_write_char(
                    shared_client->m_gattc_interface, 
                    shared_client->m_connection_id,
                    m_characteristic.char_handle,
                    data.size(),
                    data.data(),
                    ESP_GATT_WRITE_TYPE_RSP,
                    ESP_GATT_AUTH_REQ_NONE);
                result != ESP_OK)
            {
                ESP_LOGE(TAG, "Write characteristic failed.");
                return result_type::failure(errc::error);
            }

            EventBits_t bits = xEventGroupWaitBits(
                shared_client->m_event_group, 
                client::WRITE_CHAR_BIT | client::FAIL_BIT, 
                pdTRUE, 
                pdFALSE, 
                static_cast<TickType_t>(BLE_TIMEOUT));

            if (bits & client::WRITE_CHAR_BIT)
            {
                ESP_LOGI(TAG, "Write characteristic success.");
                return result_type::success();
            }
            else if (bits & client::FAIL_BIT)
            {
                ESP_LOGE(TAG, "Write characteristic failed.");
                return result_type::failure(errc::error);
            }
            else
            {
                ESP_LOGE(TAG, "Write characteristic timeout.");
                return result_type::failure(errc::timeout);
            }
        }
    }

    result<std::vector<uint8_t>> characteristic::read() const noexcept
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        using result_type = result<std::vector<uint8_t>>;

        auto shared_client = m_client_ptr.lock();

        if (!shared_client)
        {
            ESP_LOGE(TAG, "Client is not connected.");
            return result_type::failure(errc::not_connected);
        }

        {
            async::lock lock{ shared_client->m_mutex };

            if (esp_err_t result = esp_ble_gattc_read_char(
                    shared_client->m_gattc_interface, 
                    shared_client->m_connection_id,
                    m_characteristic.char_handle,
                    ESP_GATT_AUTH_REQ_NONE); 
                result != ESP_OK)
            {
                ESP_LOGE(TAG, "Read characteristic failed.");
                return result_type::failure(errc::error);
            }

            EventBits_t bits = xEventGroupWaitBits(
                shared_client->m_event_group, 
                client::READ_CHAR_BIT | client::FAIL_BIT, 
                pdTRUE, 
                pdFALSE, 
                static_cast<TickType_t>(BLE_TIMEOUT));

            if (bits & client::READ_CHAR_BIT)
            {
                ESP_LOGI(TAG, "Read characteristic success.");
                return result_type::success(std::move(shared_client->m_characteristic_data_cache));
            }
            else if (bits & client::FAIL_BIT)
            {
                ESP_LOGE(TAG, "Read characteristic failed.");
                return result_type::failure(errc::error);
            }
            else
            {
                ESP_LOGE(TAG, "Read characteristic timeout.");
                return result_type::failure(errc::error);
            }
        }
    }

    result<std::vector<descriptor>> characteristic::get_descriptors() const noexcept
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        using result_type = result<std::vector<descriptor>>;

        esp_gatt_status_t result = ESP_GATT_OK;
        uint16_t descriptor_count = 0U;
        std::vector<esp_gattc_descr_elem_t> descriptors;

        auto shared_client = m_client_ptr.lock();

        if (!shared_client)
        {
            ESP_LOGE(TAG, "Client is not connected.");
            return result_type::failure(errc::not_connected);
        }

        {
            async::lock lock{ shared_client->m_mutex };

            result = esp_ble_gattc_get_attr_count(
                shared_client->m_gattc_interface, 
                shared_client->m_connection_id,
                ESP_GATT_DB_DESCRIPTOR,
                0,
                0,
                m_characteristic.char_handle,
                &descriptor_count);

            if (result != ESP_GATT_OK)
            {
                ESP_LOGE(TAG, "Could not get desriptor count.");
                result_type::failure(errc::error);
            }

            descriptors.resize(descriptor_count);

            result = esp_ble_gattc_get_all_descr(
                shared_client->m_gattc_interface, 
                shared_client->m_connection_id,
                m_characteristic.char_handle,
                descriptors.data(),
                &descriptor_count,
                0);

            if (result != ESP_GATT_OK)
            {
                ESP_LOGE(TAG, "Could not retrieve desriptors.");
                result_type::failure(errc::error);
            }

            {
                std::vector<descriptor> result;
                result.reserve(descriptors.size());

                std::transform(descriptors.cbegin(), descriptors.cend(), std::back_inserter(result), [this](auto elem) { 
                    return descriptor(m_client_ptr, elem); 
                });

                return result_type::success(std::move(result));
            }
        }
    }

    result<descriptor> characteristic::get_descriptor_by_uuid(const esp_bt_uuid_t* uuid) const noexcept
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        using result_type = result<descriptor>;

        esp_gatt_status_t result = ESP_GATT_OK;
        uint16_t descriptor_count = 0U;
        std::vector<esp_gattc_descr_elem_t> descriptors;

        auto shared_client = m_client_ptr.lock();

        if (!shared_client)
        {
            ESP_LOGE(TAG, "Client is not connected.");
            return result_type::failure(errc::not_connected);
        }

        {
            async::lock lock{ shared_client->m_mutex };

            result = esp_ble_gattc_get_attr_count(
                shared_client->m_gattc_interface, 
                shared_client->m_connection_id,
                ESP_GATT_DB_DESCRIPTOR,
                0,
                0,
                m_characteristic.char_handle,
                &descriptor_count);

            if (result != ESP_GATT_OK)
            {
                ESP_LOGE(TAG, "Could not get desriptor count.");
                return result_type::failure(errc::error);
            }

            descriptors.resize(descriptor_count);

            result = esp_ble_gattc_get_descr_by_uuid(
                shared_client->m_gattc_interface, 
                shared_client->m_connection_id,
                m_service_handle_range.first,
                m_service_handle_range.second,
                m_characteristic.uuid,
                *uuid,
                descriptors.data(),
                &descriptor_count);

            if (result != ESP_GATT_OK)
            {
                ESP_LOGE(TAG, "Could not retrieve desriptors.");
                return result_type::failure(errc::error);
            }

            return result_type::success(descriptor(m_client_ptr, std::move(descriptors.front())));
        }
    }

    result<void> characteristic::subscribe(event::notify_function_t callback) noexcept
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        using result_type = result<void>;

        auto shared_client = m_client_ptr.lock();

        if (!shared_client)
        {
            ESP_LOGE(TAG, "Client is not connected.");
            return result_type::failure(errc::not_connected);
        }

        {
            async::lock lock{ shared_client->m_mutex };

            if (esp_err_t result = esp_ble_gattc_register_for_notify(
                    shared_client->m_gattc_interface, 
                    shared_client->m_address.to_address(), 
                    m_characteristic.char_handle); 
                result != ESP_OK)
            {
                ESP_LOGE(TAG, "Subscribe to characteristic failed.");
                return result_type::failure(errc::error);
            }

            EventBits_t bits = xEventGroupWaitBits(
                shared_client->m_event_group, 
                client::REG_FOR_NOTIFY_BIT | client::FAIL_BIT, 
                pdTRUE, 
                pdFALSE, 
                static_cast<TickType_t>(BLE_TIMEOUT));

            if (bits & client::REG_FOR_NOTIFY_BIT)
            {
                shared_client->m_characteristics_callbacks[m_characteristic.char_handle] += callback;
                ESP_LOGI(TAG, "Subscribe to characteristic success.");
                return result_type::success();
            }
            else if (bits & client::FAIL_BIT)
            {
                ESP_LOGE(TAG, "Subscribe to characteristic failed.");
                return result_type::failure(errc::error);
            }
            else
            {
                ESP_LOGE(TAG, "Subscribe to characteristic timeout.");
                return result_type::failure(errc::timeout);
            }
        }
    }

    result<void> characteristic::unsubscribe() noexcept
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        using result_type = result<void>;

        auto shared_client = m_client_ptr.lock();

        if (!shared_client)
        {
            ESP_LOGE(TAG, "Client is not connected.");
            return result_type::failure(errc::not_connected);
        }

        {
            async::lock lock{ shared_client->m_mutex };
        
            if (esp_err_t result = esp_ble_gattc_unregister_for_notify(
                    shared_client->m_gattc_interface, 
                    shared_client->m_address.to_address(), 
                    m_characteristic.char_handle); 
                result != ESP_OK)
            {
                ESP_LOGI(TAG, "Unsubscribe from characteristic success.");
                return result_type::success();
            }

            EventBits_t bits = xEventGroupWaitBits(
                shared_client->m_event_group, 
                client::UNREG_FOR_NOTIFY_BIT | client::FAIL_BIT, 
                pdTRUE, 
                pdFALSE, 
                static_cast<TickType_t>(BLE_TIMEOUT));

            if (bits & client::UNREG_FOR_NOTIFY_BIT)
            {
                shared_client->m_characteristics_callbacks.erase(m_characteristic.char_handle);
                ESP_LOGI(TAG, "Unsubscribe from characteristic success.");
                return result_type::success();
            }
            else if (bits & client::FAIL_BIT)
            {
                ESP_LOGE(TAG, "Unsubscribe from characteristic failed.");
                return result_type::failure(errc::error);
            }
            else
            {
                ESP_LOGE(TAG, "Unsubscribe from characteristic timeout.");
                return result_type::failure(errc::timeout);
            }
        }
    }
}