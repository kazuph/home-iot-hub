#include "hub_ble.h"
#include "hub_timing.h"

#include <cstring>
#include <memory>
#include <algorithm>
#include <vector>
#include <charconv>

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"
#include "esp_gap_ble_api.h"

#include "esp_log.h"

namespace hub::ble
{
    using namespace timing::literals;

    constexpr const char* TAG{ "HUB_BLE" };
    
    constexpr auto BLE_TIMEOUT      { 10_s };
    constexpr EventBits_t FAIL_BIT  { BIT15 };

    /* Bits used by GAP */
    constexpr EventBits_t SCAN_START_BIT    { BIT0 };
    constexpr EventBits_t SCAN_STOP_BIT     { BIT1 };

    constexpr esp_ble_scan_params_t ble_scan_params{
        BLE_SCAN_TYPE_ACTIVE,           // Scan type
        BLE_ADDR_TYPE_PUBLIC,           // Address type
        BLE_SCAN_FILTER_ALLOW_ALL,      // Filter policy
        0x50,                           // Scan interval
        0x30,                           // Scan window
        BLE_SCAN_DUPLICATE_DISABLE      // Advertise duplicates filter policy
    };

    static void esp_gap_callback(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);

    static EventGroupHandle_t scan_event_group = nullptr;

    scan_results_event_handler_t scan_results_event_handler{};

    std::array<client*, CONFIG_BTDM_CTRL_BLE_MAX_CONN>  client::clients{};

    static void esp_gap_callback(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
        {
            ESP_LOGD(TAG, "Function: %s.", __func__);

            switch (event)
            {
            case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT:
                ESP_LOGV(TAG, "ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT");
                break;
            case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
                ESP_LOGV(TAG, "ESP_GAP_BLE_SCAN_START_COMPLETE_EVT");

                if (param->scan_start_cmpl.status != ESP_BT_STATUS_SUCCESS)
                {
                    xEventGroupSetBits(scan_event_group, FAIL_BIT);
                    ESP_LOGE(TAG, "Scan start failed with error code %x.", param->scan_start_cmpl.status);
                    break;
                }

                xEventGroupSetBits(scan_event_group, SCAN_START_BIT);
                break;
            case ESP_GAP_BLE_SCAN_RESULT_EVT:
                ESP_LOGV(TAG, "ESP_GAP_BLE_SCAN_RESULT_EVT");
                switch (param->scan_rst.search_evt)
                {
                case ESP_GAP_SEARCH_INQ_RES_EVT:
                    ESP_LOGV(TAG, "ESP_GAP_SEARCH_INQ_RES_EVT");
                    {
                        uint8_t adv_name_len = 0;
                        uint8_t* adv_name = esp_ble_resolve_adv_data(param->scan_rst.ble_adv, ESP_BLE_AD_TYPE_NAME_CMPL, &adv_name_len);

                        if (adv_name == nullptr || adv_name_len == 0)
                        {
                            break;
                        }

                        if (!scan_results_event_handler)
                        {
                            break;
                        }

                        scan_results_event_handler.invoke(
                            nullptr, 
                            { 
                                std::string(reinterpret_cast<const char*>(adv_name), static_cast<size_t>(adv_name_len)), 
                                mac(param->scan_rst.bda, param->scan_rst.ble_addr_type) 
                            }
                        );
                    }
                    break;
                case ESP_GAP_SEARCH_INQ_CMPL_EVT:
                    ESP_LOGV(TAG, "ESP_GAP_SEARCH_INQ_CMPL_EVT");
                    break;
                default: break;
                }
                break;
            case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
                ESP_LOGV(TAG, "ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT");
                if (param->scan_stop_cmpl.status != ESP_BT_STATUS_SUCCESS)
                {
                    xEventGroupSetBits(scan_event_group, FAIL_BIT);
                    ESP_LOGE(TAG, "Scan stop failed with error code %x.", param->scan_stop_cmpl.status);
                    break;
                }

                xEventGroupSetBits(scan_event_group, SCAN_STOP_BIT);
                break;
            case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
                ESP_LOGV(TAG, "ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT");
                break;
            default:
                ESP_LOGW(TAG, "Other GAP event: %x.", event);
                break;
            }
        }

    esp_err_t init()
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        esp_err_t result = ESP_OK;

        esp_bt_controller_config_t bt_config = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
        result = esp_bt_controller_init(&bt_config);
        if (result != ESP_OK)
        {
            ESP_LOGE(TAG, "BLE initialization failed with error code %x [%s].", result, esp_err_to_name(result));
            return result;
        }

        result = esp_bt_controller_enable(ESP_BT_MODE_BLE);
        if (result != ESP_OK)
        {
            ESP_LOGE(TAG, "BLE controller enable failed with error code %x [%s].", result, esp_err_to_name(result));
            goto cleanup_bt_controller_init;
        }

        result = esp_bluedroid_init();
        if (result != ESP_OK)
        {
            ESP_LOGE(TAG, "Bluedroid initialization failed with error code %x [%s].", result, esp_err_to_name(result));
            goto cleanup_bt_controller_enable;
        }

        result = esp_bluedroid_enable();
        if (result != ESP_OK)
        {
            ESP_LOGE(TAG, "Bluedroid enable failed with error code %x [%s].", result, esp_err_to_name(result));
            goto cleanup_bluedroid_init;
        }

        result = esp_ble_gap_register_callback(&esp_gap_callback);
        if (result != ESP_OK)
        {
            ESP_LOGE(TAG, "BLE GAP callback register failed with error code %x [%s].", result, esp_err_to_name(result));
            goto cleanup_bluedroid_enable;
        }

        result = esp_ble_gattc_register_callback(&client::gattc_callback);
        if (result != ESP_OK)
        {
            ESP_LOGE(TAG, "BLE GATC callback register failed with error code %x [%s].", result, esp_err_to_name(result));
            goto cleanup_bluedroid_enable;
        }

        result = esp_ble_gatt_set_local_mtu(500);
        if (result != ESP_OK)
        {
            ESP_LOGE(TAG, "BLE GATT set local MTU failed with error code %x [%s].", result, esp_err_to_name(result));
            goto cleanup_bluedroid_enable;
        }

        scan_event_group = xEventGroupCreate();
        if (scan_event_group == nullptr)
        {
            ESP_LOGE(TAG, "Event group create failed.");
            goto cleanup_bluedroid_enable;
        }

        ESP_LOGI(TAG, "BLE initialized.");

        return result;

    cleanup_bluedroid_enable:
        esp_bluedroid_disable();
    cleanup_bluedroid_init:
        esp_bluedroid_deinit();
    cleanup_bt_controller_enable:
        esp_bt_controller_disable();
    cleanup_bt_controller_init:
        esp_bt_controller_deinit();

        return result;
    }

    esp_err_t deinit()
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        if (scan_event_group != nullptr)
        {
            vEventGroupDelete(scan_event_group);
        }
        
        esp_bluedroid_disable();
        esp_bluedroid_deinit();
        esp_bt_controller_disable();
        esp_bt_controller_deinit();

        return ESP_OK;
    }
    
    esp_err_t start_scanning(uint32_t scan_time)
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        esp_err_t result = esp_ble_gap_start_scanning(scan_time);

        EventBits_t bits = xEventGroupWaitBits(scan_event_group, SCAN_START_BIT | FAIL_BIT, pdTRUE, pdFALSE, static_cast<TickType_t>(BLE_TIMEOUT));

        if (bits & SCAN_START_BIT)
        {
            ESP_LOGI(TAG, "Scan started successfully.");
        }
        else if (bits & FAIL_BIT)
        {
            result = ESP_FAIL;
            ESP_LOGE(TAG, "Scan start failed with error code %x [%s].", result, esp_err_to_name(result));
        }
        else
        {
            result = ESP_ERR_TIMEOUT;
            ESP_LOGE(TAG, "Scan start failed with error code %x [%s].", result, esp_err_to_name(result));
        }

        return result;
    }

    esp_err_t stop_scanning()
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        esp_err_t result = esp_ble_gap_stop_scanning();

        EventBits_t bits = xEventGroupWaitBits(scan_event_group, SCAN_STOP_BIT | FAIL_BIT, pdTRUE, pdFALSE, static_cast<TickType_t>(BLE_TIMEOUT));

        if (bits & SCAN_STOP_BIT)
        {
            ESP_LOGI(TAG, "Scan stopped successfully.");
        }
        else if (bits & FAIL_BIT)
        {
            result = ESP_FAIL;
            ESP_LOGE(TAG, "Scan stop failed with error code %x [%s].", result, esp_err_to_name(result));
        }
        else
        {
            result = ESP_ERR_TIMEOUT;
            ESP_LOGE(TAG, "Scan stop failed with error code %x [%s].", result, esp_err_to_name(result));
        }

        return result;
    }

    void client::gattc_callback(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param)
    {
        ESP_LOGD(TAG, "Function: %s", __func__);

        esp_err_t result = ESP_OK;

        if (event == ESP_GATTC_REG_EVT)
        {
            if (param->reg.status == ESP_GATT_OK)
            {
                ESP_LOGI(TAG, "Register app_id: %04x.", param->reg.app_id);

                client* const ble_client = client::clients[param->reg.app_id];
                ble_client->gattc_if = gattc_if;

                result = esp_ble_gap_set_scan_params(const_cast<esp_ble_scan_params_t*>(&ble_scan_params));
                if (result != ESP_OK)
                {
                    ESP_LOGE(TAG, "Set scan parameters failed with error code %x [%s].", result, esp_err_to_name(result));
                    return;
                }
            }
            else
            {
                ESP_LOGE(TAG, "Register app failed, app_id %04x, status %d", param->reg.app_id, param->reg.status);
                return;
            }

            return;
        }

        client* const ble_client = get_client(gattc_if);
        if (!ble_client)
        {
            ESP_LOGE(TAG, "Client not found.");
            return;
        }

        switch (event)
        {
        case ESP_GATTC_NOTIFY_EVT:
            ESP_LOGV(TAG, "ESP_GATTC_NOTIFY_EVT");

            if (!ble_client->notify_event_handler)
            {
                break;
            }

            ble_client->notify_event_handler.invoke(
                ble_client, 
                {
                    param->notify.handle,
                    std::string(reinterpret_cast<const char*>(param->notify.value), static_cast<size_t>(param->notify.value_len))
                }
            );

            break;
        case ESP_GATTC_CONNECT_EVT:
            ESP_LOGV(TAG, "ESP_GATTC_CONNECT_EVT");

            ble_client->conn_id = param->connect.conn_id;
            ble_client->address = mac(param->connect.remote_bda);

            result = esp_ble_gattc_send_mtu_req(gattc_if, ble_client->conn_id);
            if (result != ESP_OK)
            {
                xEventGroupSetBits(ble_client->event_group, FAIL_BIT);
                ESP_LOGE(TAG, "MTU configuration failed with error code %x [%s].", result, esp_err_to_name(result));
                break;
            }
            ESP_LOGI(TAG, "MTU configuration success.");

            if (!ble_client->connect_event_handler)
            {
                break;
            }

            ble_client->connect_event_handler.invoke(ble_client, {});

            break;
        case ESP_GATTC_OPEN_EVT:
            ESP_LOGV(TAG, "ESP_GATTC_OPEN_EVT");
            break;
        case ESP_GATTC_DIS_SRVC_CMPL_EVT:
            ESP_LOGV(TAG, "ESP_GATTC_DIS_SRVC_CMPL_EVT");
            break;
        case ESP_GATTC_CFG_MTU_EVT:
            ESP_LOGV(TAG, "ESP_GATTC_CFG_MTU_EVT");
            xEventGroupSetBits(ble_client->event_group, CONNECT_BIT);
            break;
        case ESP_GATTC_SEARCH_RES_EVT:
            ESP_LOGV(TAG, "ESP_GATTC_SEARCH_RES_EVT");
            ble_client->service_start_handle = param->search_res.start_handle;
            ble_client->service_end_handle = param->search_res.end_handle;
            break;
        case ESP_GATTC_SEARCH_CMPL_EVT:
            ESP_LOGV(TAG, "ESP_GATTC_SEARCH_CMPL_EVT");
            xEventGroupSetBits(ble_client->event_group, SEARCH_SERVICE_BIT);
            break;
        case ESP_GATTC_REG_FOR_NOTIFY_EVT:
            ESP_LOGV(TAG, "ESP_GATTC_REG_FOR_NOTIFY_EVT");
            xEventGroupSetBits(ble_client->event_group, REG_FOR_NOTIFY_BIT);
            break;
        case ESP_GATTC_UNREG_FOR_NOTIFY_EVT:
            ESP_LOGV(TAG, "ESP_GATTC_UNREG_FOR_NOTIFY_EVT");
            xEventGroupSetBits(ble_client->event_group, UNREG_FOR_NOTIFY_BIT);
            break;
        case ESP_GATTC_WRITE_CHAR_EVT:
            ESP_LOGV(TAG, "ESP_GATTC_WRITE_CHAR_EVT");
            xEventGroupSetBits(ble_client->event_group, WRITE_CHAR_BIT);
            break;
        case ESP_GATTC_READ_CHAR_EVT:
            ESP_LOGV(TAG, "ESP_GATTC_READ_CHAR_EVT");

            if (ble_client->buff_length == nullptr)
            {
                ESP_LOGW(TAG, "Buffer length pointer is nullptr.");
                xEventGroupSetBits(ble_client->event_group, READ_CHAR_BIT);
                break;
            }

            // If the length of the buffer is not set by the previous call, we only provide a required buffer length
            if (*(ble_client->buff_length) != param->read.value_len)
            {
                *(ble_client->buff_length) = param->read.value_len;
                xEventGroupSetBits(ble_client->event_group, READ_CHAR_BIT);
                break;
            }

            if (ble_client->buff == nullptr)
            {
                ESP_LOGW(TAG, "Buffer not pointer is nullptr.");
                xEventGroupSetBits(ble_client->event_group, READ_CHAR_BIT);
                break;
            }

            // If the buffer is prepared correctly and the length matches, we copy the content of the characteristic
            std::copy(param->read.value, param->read.value + *(ble_client->buff_length), reinterpret_cast<uint8_t*>(ble_client->buff));

            xEventGroupSetBits(ble_client->event_group, READ_CHAR_BIT);
            break;
        case ESP_GATTC_WRITE_DESCR_EVT:
            ESP_LOGV(TAG, "ESP_GATTC_WRITE_DESCR_EVT");
            xEventGroupSetBits(ble_client->event_group, WRITE_DESCR_BIT);
            break;
        case ESP_GATTC_READ_DESCR_EVT:
            ESP_LOGV(TAG, "ESP_GATTC_READ_DESCR_EVT");

            if (ble_client->buff_length == nullptr)
            {
                ESP_LOGW(TAG, "Buffer length pointer is nullptr.");
                xEventGroupSetBits(ble_client->event_group, READ_DESCR_BIT);
                break;
            }

            // If the length of the buffer is not set by the previous call, we only provide a required buffer length
            if (*(ble_client->buff_length) != param->read.value_len)
            {
                *(ble_client->buff_length) = param->read.value_len;
                xEventGroupSetBits(ble_client->event_group, READ_DESCR_BIT);
                break;
            }

            if (ble_client->buff == nullptr)
            {
                ESP_LOGW(TAG, "Buffer pointer is nullptr.");
                xEventGroupSetBits(ble_client->event_group, READ_DESCR_BIT);
                break;
            }

            // If the buffer is prepared correctly and the length matches, we copy the content of the descriptor
            std::copy(param->read.value, param->read.value + *(ble_client->buff_length), reinterpret_cast<uint8_t*>(ble_client->buff));

            xEventGroupSetBits(ble_client->event_group, READ_DESCR_BIT);
            break;
        case ESP_GATTC_SRVC_CHG_EVT:
            ESP_LOGV(TAG, "ESP_GATTC_SRVC_CHG_EVT");
            break;
        case ESP_GATTC_DISCONNECT_EVT:
            ESP_LOGV(TAG, "ESP_GATTC_DISCONNECT_EVT");
            ESP_LOGI(TAG, "Disconnect reason: %#04x", param->disconnect.reason);
            xEventGroupSetBits(ble_client->event_group, DISCONNECT_BIT);

            // Do not call the callback function if disconnect was requested by user
            if (param->disconnect.reason == ESP_GATT_CONN_TERMINATE_LOCAL_HOST)
            {
                break;
            }

            if (!ble_client->disconnect_event_handler)
            {
                break;
            }

            ble_client->disconnect_event_handler.invoke(ble_client, {});

            break;
        case ESP_GATTC_CLOSE_EVT:
            ESP_LOGV(TAG, "ESP_GATTC_CLOSE_EVT");
            break;
        default:
            ESP_LOGW(TAG, "Other GATTC event: %i.", event);
            break;
        }
    }

    client* client::get_client(const esp_gatt_if_t gattc_if)
    {
        ESP_LOGD(TAG, "Function: %s", __func__);

        auto iter = std::find_if(clients.begin(), clients.end(), [gattc_if](const auto& elem) { 
            return (elem != nullptr && elem->gattc_if == gattc_if); 
        });

        return (iter != clients.end()) ? *iter : nullptr;
    }

    client::client() :
        connect_event_handler{},
        disconnect_event_handler{},
        notify_event_handler{},
        app_id{ 0 }, 
        gattc_if{ ESP_GATT_IF_NONE }, 
        conn_id{ 0 },
        service_start_handle{ 0 },
        service_end_handle{ 0 },
        address{ "00:00:00:00:00:00" },
        event_group{ nullptr },
        buff_length{ nullptr },
        buff{ nullptr }
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        esp_err_t result = ESP_OK;

        auto iter = std::find_if(clients.begin(), clients.end(), [](const auto& elem) {
            return (elem == nullptr);
        });

        if (iter == clients.end())
        {
            ESP_LOGE(TAG, "Maximum number of devices already connected.");
            abort();
        }

        {
            uint16_t app_id = std::distance(clients.begin(), iter);

            this->app_id = app_id;

            *iter = this;

            result = esp_ble_gattc_app_register(this->app_id);
            if (result != ESP_OK)
            {
                ESP_LOGE(TAG, "Could not register app.");
                abort();
            }

            event_group = xEventGroupCreate();
            if (event_group == nullptr)
            {
                ESP_LOGE(TAG, "Event group create failed.");
                abort();
            }
        }

        ESP_LOGI(TAG, "BLE client initialized.");
    }

    client::~client()
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        clients[app_id] = nullptr;
        esp_ble_gattc_app_unregister(gattc_if);
        vEventGroupDelete(event_group);

        ESP_LOGI(TAG, "BLE client destroyed.");    
    }

    esp_err_t client::connect(const mac& address)
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        esp_err_t result = ESP_OK;

        result = stop_scanning();
        if (result != ESP_OK)
        {
            ESP_LOGW(TAG, "Scan stop failed with error code %x [%s].", result, esp_err_to_name(result));
        }

        result = esp_ble_gattc_open(
            gattc_if, 
            const_cast<uint8_t*>(address.to_address()), 
            address.get_type(), 
            true);

        if (result != ESP_OK)
        {
            ESP_LOGE(TAG, "GATT connect failed with error code %x [%s].", result, esp_err_to_name(result));
            return result;
        }

        EventBits_t bits = xEventGroupWaitBits(event_group, CONNECT_BIT | FAIL_BIT, pdTRUE, pdFALSE, static_cast<TickType_t>(BLE_TIMEOUT));

        if (bits & CONNECT_BIT)
        {
            ESP_LOGI(TAG, "Connection success.");
        }
        else if (bits & FAIL_BIT)
        {
            result = ESP_FAIL;
            ESP_LOGE(TAG, "Connection failed with error code %x [%s].", result, esp_err_to_name(result));
        }
        else
        {
            result = ESP_ERR_TIMEOUT;
            ESP_LOGE(TAG, "Connection failed with error code %x [%s].", result, esp_err_to_name(result));
        }

        return result;
    }

    esp_err_t client::disconnect()
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        esp_err_t result = ESP_OK;

        result = esp_ble_gattc_close(gattc_if, conn_id);
        if (result != ESP_OK)
        {
            ESP_LOGE(TAG, "GATT disconnect failed with error code %x [%s].", result, esp_err_to_name(result));
            return result;
        }

        EventBits_t bits = xEventGroupWaitBits(event_group, DISCONNECT_BIT | FAIL_BIT, pdTRUE, pdFALSE, static_cast<TickType_t>(BLE_TIMEOUT));

        if (bits & DISCONNECT_BIT)
        {
            ESP_LOGI(TAG, "Disconnect success.");
        }
        else if (bits & FAIL_BIT)
        {
            result = ESP_FAIL;
            ESP_LOGE(TAG, "Disconnect failed with error code %x [%s].", result, esp_err_to_name(result));
        }
        else
        {
            result = ESP_ERR_TIMEOUT;
            ESP_LOGE(TAG, "Disconnect failed with error code %x [%s].", result, esp_err_to_name(result));
        }

        return result;
    }

    esp_err_t client::register_for_notify(uint16_t handle)
    {            
        ESP_LOGD(TAG, "Function: %s.", __func__);
        esp_err_t result = ESP_OK;

        result = esp_ble_gattc_register_for_notify(gattc_if, address.to_address(), handle);
        if (result != ESP_OK)
        {
            ESP_LOGE(TAG, "Register for notify failed with error code %x [%s].", result, esp_err_to_name(result));
            return result;
        }

        EventBits_t bits = xEventGroupWaitBits(event_group, REG_FOR_NOTIFY_BIT | FAIL_BIT, pdTRUE, pdFALSE, static_cast<TickType_t>(BLE_TIMEOUT));

        if (bits & REG_FOR_NOTIFY_BIT)
        {
            ESP_LOGI(TAG, "Register for notify success.");
        }
        else if (bits & FAIL_BIT)
        {
            result = ESP_FAIL;
            ESP_LOGE(TAG, "Register for notify failed with error code %x [%s].", result, esp_err_to_name(result));
        }
        else
        {
            result = ESP_ERR_TIMEOUT;
            ESP_LOGE(TAG, "Register for notify failed with error code %x [%s].", result, esp_err_to_name(result));
        }

        return result;
    }

    esp_err_t client::unregister_for_notify(uint16_t handle)
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        esp_err_t result = ESP_OK;

        result = esp_ble_gattc_unregister_for_notify(gattc_if, address.to_address(), handle);
        if (result != ESP_OK)
        {
            ESP_LOGE(TAG, "Unregister for notify failed with error code %x [%s].", result, esp_err_to_name(result));
            return result;
        }

        EventBits_t bits = xEventGroupWaitBits(event_group, UNREG_FOR_NOTIFY_BIT | FAIL_BIT, pdTRUE, pdFALSE, static_cast<TickType_t>(BLE_TIMEOUT));

        ESP_LOGI(TAG, "Unregister for notify success.");

        if (bits & UNREG_FOR_NOTIFY_BIT)
        {
            ESP_LOGI(TAG, "Unregister for notify success.");
        }
        else if (bits & FAIL_BIT)
        {
            result = ESP_FAIL;
            ESP_LOGE(TAG, "Unregister for notify failed with error code %x [%s].", result, esp_err_to_name(result));
        }
        else
        {
            result = ESP_ERR_TIMEOUT;
            ESP_LOGE(TAG, "Unregister for notify failed with error code %x [%s].", result, esp_err_to_name(result));
        }

        return result;
    }

    esp_err_t client::get_services(esp_gattc_service_elem_t* services, uint16_t* count)
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        return ESP_FAIL;
    }

    esp_err_t client::get_service(const esp_bt_uuid_t* uuid)
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        esp_err_t result = ESP_OK;

        result = esp_ble_gattc_search_service(gattc_if, conn_id, const_cast<esp_bt_uuid_t*>(uuid));
        if (result != ESP_OK)
        {
            ESP_LOGE(TAG, "Get service failed with error code %x [%s].", result, esp_err_to_name(result));
            return result;
        }

        EventBits_t bits = xEventGroupWaitBits(event_group, SEARCH_SERVICE_BIT | FAIL_BIT, pdTRUE, pdFALSE, static_cast<TickType_t>(BLE_TIMEOUT));

        if (bits & SEARCH_SERVICE_BIT)
        {
            ESP_LOGI(TAG, "Get service success.");
        }
        else if (bits & FAIL_BIT)
        {
            result = ESP_FAIL;
            ESP_LOGE(TAG, "Get service failed with error code %x [%s].", result, esp_err_to_name(result));
        }
        else
        {
            result = ESP_ERR_TIMEOUT;
            ESP_LOGE(TAG, "Get service failed with error code %x [%s].", result, esp_err_to_name(result));
        }

        return result;        
    }

    esp_err_t client::get_characteristics(esp_gattc_char_elem_t* characteristics, uint16_t* count)
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        esp_gatt_status_t result = ESP_GATT_OK;

        if (characteristics != nullptr)
        {
            result = esp_ble_gattc_get_all_char(
                gattc_if, 
                conn_id,
                service_start_handle,
                service_end_handle,
                characteristics,
                count,
                0);
        }
        else
        {
            result = esp_ble_gattc_get_attr_count(
                gattc_if, 
                conn_id,
                ESP_GATT_DB_CHARACTERISTIC,
                service_start_handle,
                service_end_handle,
                ESP_GATT_ILLEGAL_HANDLE,
                count);
        }

        if (result != ESP_GATT_OK)
        {
            ESP_LOGE(TAG, "Get characteristics failed with error code %x.\n", result);
            return ESP_FAIL;
        }

        return ESP_OK;  
    }

    esp_err_t client::write_characteristic(const uint16_t handle, const uint8_t* value, const uint16_t value_length)
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        esp_err_t result = ESP_OK;

        result = esp_ble_gattc_write_char(
            gattc_if, 
            conn_id,
            handle,
            value_length,
            const_cast<uint8_t*>(value),
            ESP_GATT_WRITE_TYPE_RSP,
            ESP_GATT_AUTH_REQ_NONE);

        if (result != ESP_OK)
        {
            ESP_LOGE(TAG, "Write characteristic failed with error code %x [%s].", result, esp_err_to_name(result));
            return result;
        }

        EventBits_t bits = xEventGroupWaitBits(event_group, WRITE_CHAR_BIT | FAIL_BIT, pdTRUE, pdFALSE, static_cast<TickType_t>(BLE_TIMEOUT));

        if (bits & WRITE_CHAR_BIT)
        {
            ESP_LOGI(TAG, "Write characteristic success.");
        }
        else if (bits & FAIL_BIT)
        {
            result = ESP_FAIL;
            ESP_LOGE(TAG, "Write characteristic failed with error code %x [%s].", result, esp_err_to_name(result));
        }
        else
        {
            result = ESP_ERR_TIMEOUT;
            ESP_LOGE(TAG, "Write characteristic failed with error code %x [%s].", result, esp_err_to_name(result));
        }

        return result;
    }

    esp_err_t client::read_characteristic(const uint16_t handle, uint8_t* value, uint16_t* value_length)
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        esp_err_t result = ESP_OK;

        buff = value;
        buff_length = value_length;

        result = esp_ble_gattc_read_char(
            gattc_if, 
            conn_id,
            handle,
            ESP_GATT_AUTH_REQ_NONE
        );

        if (result != ESP_OK)
        {
            ESP_LOGE(TAG, "Read characteristic failed with error code %x [%s].", result, esp_err_to_name(result));
            return result;
        }

        EventBits_t bits = xEventGroupWaitBits(event_group, READ_CHAR_BIT | FAIL_BIT, pdTRUE, pdFALSE, static_cast<TickType_t>(BLE_TIMEOUT));

        if (bits & READ_CHAR_BIT)
        {
            ESP_LOGI(TAG, "Read characteristic success.");
        }
        else if (bits & FAIL_BIT)
        {
            result = ESP_FAIL;
            ESP_LOGE(TAG, "Read characteristic failed with error code %x [%s].", result, esp_err_to_name(result));
        }
        else
        {
            result = ESP_ERR_TIMEOUT;
            ESP_LOGE(TAG, "Read characteristic failed with error code %x [%s].", result, esp_err_to_name(result));
        }

        buff = nullptr;
        buff_length = nullptr;

        return result;
    }

    esp_err_t client::get_descriptors(const uint16_t char_handle, esp_gattc_descr_elem_t* descr, uint16_t* count)
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        esp_gatt_status_t result = ESP_GATT_OK;

        if (descr != nullptr)
        {
            result = esp_ble_gattc_get_all_descr(
                gattc_if, 
                conn_id,
                char_handle,
                descr,
                count,
                0);
        }
        else
        {
            result = esp_ble_gattc_get_attr_count(
                gattc_if, 
                conn_id,
                ESP_GATT_DB_DESCRIPTOR,
                0,
                0,
                char_handle,
                count);
        }

        if (result != ESP_GATT_OK)
        {
            ESP_LOGE(TAG, "Get descriptors failed with error code %x.", result);
            return ESP_FAIL;
        }

        return ESP_OK;
    }

    esp_err_t client::write_descriptor(const uint16_t handle, const uint8_t* value, const uint16_t value_length)
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        esp_err_t result = ESP_OK;

        result = esp_ble_gattc_write_char_descr(
            gattc_if, 
            conn_id,
            handle,
            value_length,
            const_cast<uint8_t*>(value),
            ESP_GATT_WRITE_TYPE_RSP,
            ESP_GATT_AUTH_REQ_NONE);

        if (result != ESP_OK)
        {
            ESP_LOGE(TAG, "Write descriptor failed with error code %x [%s].", result, esp_err_to_name(result));
            return result;
        }

        EventBits_t bits = xEventGroupWaitBits(event_group, WRITE_DESCR_BIT | FAIL_BIT, pdTRUE, pdFALSE, static_cast<TickType_t>(BLE_TIMEOUT));

        if (bits & WRITE_DESCR_BIT)
        {
            ESP_LOGI(TAG, "Write descriptor success.");
        }
        else if (bits & FAIL_BIT)
        {
            result = ESP_FAIL;
            ESP_LOGE(TAG, "Write descriptor failed with error code %x [%s].", result, esp_err_to_name(result));
        }
        else
        {
            result = ESP_ERR_TIMEOUT;
            ESP_LOGE(TAG, "Write descriptor failed with error code %x [%s].", result, esp_err_to_name(result));
        }

        return result;
    }

    esp_err_t client::read_descriptor(const uint16_t handle, uint8_t* value, uint16_t* value_length)
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        esp_err_t result = ESP_OK;

        buff = value;
        buff_length = value_length;

        result = esp_ble_gattc_read_char_descr(
            gattc_if, 
            conn_id,
            handle,
            ESP_GATT_AUTH_REQ_NONE
        );

        if (result != ESP_OK)
        {
            ESP_LOGE(TAG, "Read descriptor failed with error code %x [%s].", result, esp_err_to_name(result));
            return result;
        }

        EventBits_t bits = xEventGroupWaitBits(event_group, READ_DESCR_BIT | FAIL_BIT, pdTRUE, pdFALSE, static_cast<TickType_t>(BLE_TIMEOUT));

        if (bits & READ_DESCR_BIT)
        {
            ESP_LOGI(TAG, "Read descriptor success.");
        }
        else if (bits & FAIL_BIT)
        {
            result = ESP_FAIL;
            ESP_LOGE(TAG, "Read descriptor failed with error code %x [%s].", result, esp_err_to_name(result));
        }
        else
        {
            result = ESP_ERR_TIMEOUT;
            ESP_LOGE(TAG, "Read descriptor failed with error code %x [%s].", result, esp_err_to_name(result));
        }

        buff = nullptr;
        buff_length = nullptr;

        return result;
    }
}