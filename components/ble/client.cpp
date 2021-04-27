#include "client.hpp"
#include "hub_error.h"

#include "esp_err.h"

#include <array>
#include <algorithm>

namespace hub::ble
{
    static std::array<std::weak_ptr<client>, MAX_CLIENTS> g_client_refs;

    constexpr esp_ble_scan_params_t BLE_SCAN_PARAMS{
        BLE_SCAN_TYPE_ACTIVE,           // Scan type
        BLE_ADDR_TYPE_PUBLIC,           // Address type
        BLE_SCAN_FILTER_ALLOW_ALL,      // Filter policy
        0x50,                           // Scan interval
        0x30,                           // Scan window
        BLE_SCAN_DUPLICATE_DISABLE      // Advertise duplicates filter policy
    };

    std::shared_ptr<client> client::make_client()
    {
        auto iter = std::find_if(g_client_refs.cbegin(), g_client_refs.cend(), [](const auto& client_ref) {
            return !(client_ref.expired());
        });

        if (iter == g_client_refs.cend())
        {
            return std::shared_ptr<client>();
        }

        {
            std::shared_ptr<client> client_ptr = iter->lock();

            client_ptr->m_app_id = std::distance(g_client_refs.cbegin(), iter);

            if (esp_ble_gattc_app_register(client_ptr->m_app_id) != ESP_OK)
            {
                return std::make_shared<client>();
            }

            return client_ptr;
        }
    }

    void client::gattc_callback(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param)
    {
        esp_err_t result = ESP_OK;

        auto get_shared_client = [](esp_gatt_if_t gattc_interface) {
            if (auto iter = std::find_if(
                    g_client_refs.cbegin(), 
                    g_client_refs.cend(), 
                    [gattc_interface](auto client_ref) { return client_ref->m_gattc_interface == gattc_interface; }); 
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
                    return;
                }

                {
                    std::shared_ptr<client> client_ptr = g_client_refs[param->reg.app_id].lock();
                    client_ptr->m_gattc_interface = gattc_if;
                }

                esp_ble_gap_set_scan_params(const_cast<esp_ble_scan_params_t*>(&BLE_SCAN_PARAMS));
            }

            return;
        }

        switch (event)
        {
        case ESP_GATTC_NOTIFY_EVT:
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
            xEventGroupSetBits(client_ptr->m_event_group, CONNECT_BIT);
            break;
        case ESP_GATTC_SEARCH_RES_EVT:
            client_ptr->m_services_cache.push_back(
                service(
                    client_ptr, 
                    { 
                        param->is_primary,
                        param->search_res.start_handle,
                        param->search_res.end_handle,
                        param->srvc_id.uuid
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
            std::copy(param->read.value, param->read.value + param->read.value_len, ble_client->m_descriptor_data_cache.begin());
            xEventGroupSetBits(client_ptr->m_event_group, WRITE_DESCR_BIT);
            break;
        case ESP_GATTC_READ_DESCR_EVT:
            xEventGroupSetBits(client_ptr->m_event_group, READ_DESCR_BIT);
            break;
        case ESP_GATTC_DISCONNECT_EVT:
            xEventGroupSetBits(client_ptr->m_event_group, DISCONNECT_BIT);
            break;
        default:
            break;
        }
    }

    client::~client()
    {
        clients[m_app_id].reset();
        esp_ble_gattc_app_unregister(m_gattc_interface);
        vEventGroupDelete(m_event_group);
    }

    client::client() :
        m_connection_id             { 0 },
        m_app_id                    { 0 },
        m_gattc_interface           { 0 },
        m_address                   {  },
        m_event_group               { xEventGroupCreate() }
        m_services_cache            {  },
        m_characteristic_data_cache {  },
        m_descriptor_data_cache     {  }

    {
        if (!m_event_group)
        {
            throw std::bad_alloc();
        }
    }

    void client::connect(mac address)
    {
        if (esp_err_t result = esp_ble_gattc_open(
                m_gattc_interface, 
                address.to_address(), 
                address.get_type(), 
                true); 
            result != ESP_OK)
        {
            throw hub::esp_exception(result, "GATTC open failed.");
        }

        EventBits_t bits = xEventGroupWaitBits(m_event_group, CONNECT_BIT | FAIL_BIT, pdTRUE, pdFALSE, static_cast<TickType_t>(BLE_TIMEOUT));

        if (bits & CONNECT_BIT)
        {
            ESP_LOGI(TAG, "Connection success.");
        }
        else if (bits & FAIL_BIT)
        {
            throw hub::esp_exception(ESP_FAIL, "Connecton failed.");
        }
        else
        {
            throw hub::esp_exception(ESP_ERR_TIMEOUT, "Connecton timed out.");
        }
    }

    void client::disconnect()
    {
        if (esp_err_t result = esp_ble_gattc_close(m_gattc_interface, m_connection_id); result != ESP_OK)
        {
            throw hub::esp_exception(result, "GATTC disconnect failed.");
        }

        EventBits_t bits = xEventGroupWaitBits(m_event_group, DISCONNECT_BIT | FAIL_BIT, pdTRUE, pdFALSE, static_cast<TickType_t>(BLE_TIMEOUT));

        if (bits & DISCONNECT_BIT)
        {
            ESP_LOGI(TAG, "Disconnect success.");
        }
        else if (bits & FAIL_BIT)
        {
            throw hub::esp_exception(ESP_FAIL, "Disconnection failed.");
        }
        else
        {
            throw hub::esp_exception(ESP_ERR_TIMEOUT, "Disconnection timed out.");
        }
    }

    std::vector<service> client::get_services() const
    {
        if (esp_err_t result = esp_ble_gattc_search_service(m_gattc_interface, m_connection_id, nullptr); result != ESP_OK)
        {
            throw hub::esp_exception(result, "Get services failed.");
        }

        EventBits_t bits = xEventGroupWaitBits(m_event_group, SEARCH_SERVICE_BIT | FAIL_BIT, pdTRUE, pdFALSE, static_cast<TickType_t>(BLE_TIMEOUT));

        if (bits & SEARCH_SERVICE_BIT)
        {
            ESP_LOGI(TAG, "Get service success.");
        }
        else if (bits & FAIL_BIT)
        {
            throw hub::esp_exception(ESP_FAIL, "Get services failed.");
        }
        else
        {
            throw hub::esp_exception(ESP_ERR_TIMEOUT, "Get services timed out.");
        }

        return std::move(m_services_cache);
    }
}