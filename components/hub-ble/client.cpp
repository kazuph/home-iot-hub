#include "ble/client.hpp"

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_defs.h"
#include "esp_gatt_common_api.h"
#include "esp_gap_ble_api.h"
#include "esp_err.h"
#include "esp_log.h"

#include <stdexcept>
#include <array>
#include <algorithm>

namespace hub::ble
{
    static std::array<std::weak_ptr<client>, MAX_CLIENTS> g_client_refs;

    result<std::shared_ptr<client>> client::make_client() noexcept
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        using result_type = result<std::shared_ptr<client>>;

        auto iter = std::find_if(g_client_refs.cbegin(), g_client_refs.cend(), [](const auto& client_ref) {
            return client_ref.expired();
        });

        if (iter == g_client_refs.cend())
        {
            ESP_LOGE(TAG, "Maximum number of clients already connected.");
            return result_type::failure(errc::no_resources);
        }

        if (esp_ble_gattc_register_callback(&client::gattc_callback) != ESP_OK)
        {
            ESP_LOGE(TAG, "Could not register GATTC callback.");
            return result_type::failure(errc::error);
        }

        if (esp_ble_gatt_set_local_mtu(500) != ESP_OK)
        {
            ESP_LOGE(TAG, "Could set local MTU size.");
            return result_type::failure(errc::error);
        }

        {
            auto client_ptr = std::shared_ptr<client>(nullptr);

            try
            {
                client_ptr = std::make_shared<client>();
            }
            catch(const std::bad_alloc& err)
            {
                return result_type::failure(errc::no_resources);
            }
            
            client_ptr->m_app_id    = std::distance(g_client_refs.cbegin(), iter);
            g_client_refs[client_ptr->m_app_id] = client_ptr;

            if (esp_ble_gattc_app_register(client_ptr->m_app_id) != ESP_OK)
            {
                ESP_LOGE(TAG, "Could not register GATTC app.");
                return result_type::failure(errc::error);
            }

            return result_type::success(client_ptr);
        }
    }

    client::client() :
        m_connection_id             { 0 },
        m_app_id                    { 0 },
        m_gattc_interface           { ESP_GATT_IF_NONE },
        m_address                   {  },
        m_event_group               { xEventGroupCreate() },
        m_services_cache            {  },
        m_characteristic_data_cache {  },
        m_descriptor_data_cache     {  },
        m_characteristics_callbacks {  }
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        if (!m_event_group)
        {
            throw std::bad_alloc();
        }
    }

    client::~client()
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        g_client_refs[m_app_id].reset();

        if (m_gattc_interface != ESP_GATT_IF_NONE)
        {
            esp_ble_gattc_app_unregister(m_gattc_interface);
        }

        vEventGroupDelete(m_event_group);
    }

    void client::gattc_callback(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param) noexcept
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        ESP_LOGI(TAG, "Event: %x.", event);

        auto get_shared_client = [](esp_gatt_if_t gattc_interface) {
            if (auto iter = std::find_if(
                    g_client_refs.cbegin(), 
                    g_client_refs.cend(), 
                    [gattc_interface](auto client_ref) { return !client_ref.expired() && client_ref.lock()->m_gattc_interface == gattc_interface; }); 
                iter != g_client_refs.cend())
            {
                return iter->lock();
            }

            return std::shared_ptr<client>();
        };

        std::shared_ptr<client> client_ptr = get_shared_client(gattc_if);
        
        if (!client_ptr)
        {
            // If the client is not found, it may be not registered at this point. Check if callback is due to registration request.
            if (event == ESP_GATTC_REG_EVT)
            {
                if (param->reg.status != ESP_GATT_OK)
                {
                    ESP_LOGE(TAG, "GATTC register failed with error code: %04x.", param->reg.status);
                    return;
                }

                ESP_LOGI(TAG, "Registered GATTC app ID: %04x.", param->reg.app_id);

                {
                    std::shared_ptr<client> client_ptr = g_client_refs[param->reg.app_id].lock();

                    if (!client_ptr)
                    {
                        ESP_LOGW(TAG, "Client not found.");
                        return;
                    }

                    ESP_LOGI(TAG, "GATTC interface is: %x", gattc_if);
                    client_ptr->m_gattc_interface = gattc_if;
                }
            }

            return;
        }

        switch (event)
        {
        case ESP_GATTC_NOTIFY_EVT:
            if (auto characteristic_iter = client_ptr->m_characteristics_callbacks.find(param->notify.handle); 
                characteristic_iter != client_ptr->m_characteristics_callbacks.end())
            {
                characteristic_iter->second.invoke(std::vector<uint8_t>(param->notify.value, param->notify.value + static_cast<size_t>(param->notify.value_len)));
            }
            break;
        case ESP_GATTC_CONNECT_EVT:
            client_ptr->m_connection_id     = param->connect.conn_id;
            client_ptr->m_address           = mac(param->connect.remote_bda);

            if (esp_ble_gattc_send_mtu_req(gattc_if, client_ptr->m_connection_id) != ESP_OK)
            {
                xEventGroupSetBits(client_ptr->m_event_group, FAIL_BIT);
            }

            break;
        case ESP_GATTC_CFG_MTU_EVT:
            client_ptr->m_self_ref = client_ptr;
            xEventGroupSetBits(client_ptr->m_event_group, CONNECT_BIT);
            break;
        case ESP_GATTC_SEARCH_RES_EVT:
            client_ptr->m_services_cache.push_back(
                service(
                    client_ptr, 
                    { 
                        param->search_res.is_primary,
                        param->search_res.start_handle,
                        param->search_res.end_handle,
                        param->search_res.srvc_id.uuid
                    }
                )
            );
            break;
        case ESP_GATTC_SEARCH_CMPL_EVT:
            xEventGroupSetBits(client_ptr->m_event_group, SEARCH_SERVICE_BIT);
            break;
        case ESP_GATTC_REG_FOR_NOTIFY_EVT:
            xEventGroupSetBits(client_ptr->m_event_group, REG_FOR_NOTIFY_BIT);
            break;
        case ESP_GATTC_UNREG_FOR_NOTIFY_EVT:
            xEventGroupSetBits(client_ptr->m_event_group, UNREG_FOR_NOTIFY_BIT);
            break;
        case ESP_GATTC_WRITE_CHAR_EVT:
            xEventGroupSetBits(client_ptr->m_event_group, WRITE_CHAR_BIT);
            break;
        case ESP_GATTC_READ_CHAR_EVT:
            client_ptr->m_characteristic_data_cache.resize(param->read.value_len);
            std::copy(param->read.value, param->read.value + param->read.value_len, client_ptr->m_characteristic_data_cache.begin());
            xEventGroupSetBits(client_ptr->m_event_group, READ_CHAR_BIT);
            break;
        case ESP_GATTC_WRITE_DESCR_EVT:
            client_ptr->m_descriptor_data_cache.resize(param->read.value_len);
            std::copy(param->read.value, param->read.value + param->read.value_len, client_ptr->m_descriptor_data_cache.begin());
            xEventGroupSetBits(client_ptr->m_event_group, WRITE_DESCR_BIT);
            break;
        case ESP_GATTC_READ_DESCR_EVT:
            xEventGroupSetBits(client_ptr->m_event_group, READ_DESCR_BIT);
            break;
        case ESP_GATTC_DISCONNECT_EVT:
            xEventGroupSetBits(client_ptr->m_event_group, DISCONNECT_BIT);
            break;
        case ESP_GATTC_CLOSE_EVT:
            client_ptr->m_self_ref.reset();
            break;
        default:
            break;
        }
    }

    result<void> client::connect(mac address) noexcept
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        using result_type = result<void>;

        async::lock lock{ m_mutex };

        if (esp_err_t result = esp_ble_gattc_open(
                m_gattc_interface, 
                address.to_address(), 
                address.get_type(), 
                true); 
            result != ESP_OK)
        {
            ESP_LOGE(TAG, "GATTC open failed.");
            return result_type::failure(result);
        }

        EventBits_t bits = xEventGroupWaitBits(m_event_group, CONNECT_BIT | FAIL_BIT, pdTRUE, pdFALSE, static_cast<TickType_t>(BLE_TIMEOUT));

        if (bits & CONNECT_BIT)
        {
            ESP_LOGI(TAG, "GATT client connected.");
            return result_type::success();
        }
        else if (bits & FAIL_BIT)
        {
            ESP_LOGE(TAG, "GATT client connecton failed.");
            return result_type::failure(errc::error);
        }
        else
        {
            ESP_LOGE(TAG, "GATT client connecton timed out.");
            return result_type::failure(errc::timeout);
        }
    }

    result<void> client::disconnect() noexcept
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        using result_type = result<void>;

        async::lock lock{ m_mutex };

        if (esp_err_t result = esp_ble_gattc_close(m_gattc_interface, m_connection_id); result != ESP_OK)
        {
            ESP_LOGE(TAG, "GATTC disconnect failed.");
            return result_type::failure(errc::error);
        }

        EventBits_t bits = xEventGroupWaitBits(m_event_group, DISCONNECT_BIT | FAIL_BIT, pdTRUE, pdFALSE, static_cast<TickType_t>(BLE_TIMEOUT));

        if (bits & DISCONNECT_BIT)
        {
            ESP_LOGI(TAG, "Disconnect success.");
        }
        else if (bits & FAIL_BIT)
        {
            ESP_LOGE(TAG, "GATTC disconnect failed.");
            result_type::failure(errc::error);
        }
        else
        {
            ESP_LOGE(TAG, "GATTC disconnect timeout.");
            return result_type::failure(errc::timeout);
        }

        return result_type::success();
    }

    result<std::vector<service>> client::get_services() const noexcept
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        using result_type = result<std::vector<service>>;

        async::lock lock{ m_mutex };

        m_services_cache = std::move(std::vector<service>());
        
        if (esp_err_t result = esp_ble_gattc_search_service(m_gattc_interface, m_connection_id, nullptr); result != ESP_OK)
        {
            return result_type::failure(errc::error);
        }

        EventBits_t bits = xEventGroupWaitBits(m_event_group, SEARCH_SERVICE_BIT | FAIL_BIT, pdTRUE, pdFALSE, static_cast<TickType_t>(BLE_TIMEOUT));

        if (bits & SEARCH_SERVICE_BIT)
        {
            ESP_LOGI(TAG, "Get service success.");
        }
        else if (bits & FAIL_BIT)
        {
            ESP_LOGE(TAG, "Retrieve service failed.");
            return result_type::failure(errc::error);
        }
        else
        {
            ESP_LOGE(TAG, "Retrieve service timeout.");
            result_type::failure(errc::timeout);
        }

        if (m_services_cache.empty())
        {
            ESP_LOGE(TAG, "No service found.");
            return result_type::failure(errc::error);
        }

        return result_type::success(std::move(m_services_cache));
    }

    result<service> client::get_service_by_uuid(const esp_bt_uuid_t* uuid) const noexcept
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        using result_type = result<service>;

        async::lock lock{ m_mutex };

        m_services_cache = std::move(std::vector<service>());
        
        if (esp_err_t result = esp_ble_gattc_search_service(m_gattc_interface, m_connection_id, const_cast<esp_bt_uuid_t*>(uuid)); result != ESP_OK)
        {
            return result_type::failure(errc::error);
        }

        EventBits_t bits = xEventGroupWaitBits(m_event_group, SEARCH_SERVICE_BIT | FAIL_BIT, pdTRUE, pdFALSE, static_cast<TickType_t>(BLE_TIMEOUT));

        if (bits & SEARCH_SERVICE_BIT)
        {
            ESP_LOGI(TAG, "Get service success.");
        }
        else if (bits & FAIL_BIT)
        {
            ESP_LOGE(TAG, "Retrieve service failed.");
            return result_type::failure(errc::error);
        }
        else
        {
            ESP_LOGE(TAG, "Retrieve service timeout.");
            return result_type::failure(errc::timeout);
        }

        if (m_services_cache.empty())
        {
            ESP_LOGE(TAG, "No service found.");
            return result_type::failure(errc::error);
        }

        return result_type::success(std::move(m_services_cache.front()));
    }
}