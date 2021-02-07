#include "hub_ble.h"

#include <cstring>
#include <memory>
#include <algorithm>
#include <vector>
#include <charconv>

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"
#include "esp_gattc_api.h"
#include "esp_gap_ble_api.h"

#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

namespace hub::ble
{
    constexpr const char* TAG = "HUB_BLE";
    
    constexpr TickType_t BLE_TIMEOUT{ (TickType_t)10000 / portTICK_PERIOD_MS };
    constexpr EventBits_t FAIL_BIT{ BIT15 };

    /* Bits used by client */
    constexpr EventBits_t CONNECT_BIT{ BIT0 };
    constexpr EventBits_t SEARCH_SERVICE_BIT{ BIT1 };
    constexpr EventBits_t WRITE_CHAR_BIT{ BIT2 };
    constexpr EventBits_t READ_CHAR_BIT{ BIT3 };
    constexpr EventBits_t WRITE_DESCR_BIT{ BIT4 };
    constexpr EventBits_t READ_DESCR_BIT{ BIT5 };
    constexpr EventBits_t REG_FOR_NOTIFY_BIT{ BIT6 };
    constexpr EventBits_t UNREG_FOR_NOTIFY_BIT{ BIT7 };
    constexpr EventBits_t DISCONNECT_BIT{ BIT8 };

    /* Bits used by GAP */
    constexpr EventBits_t SCAN_START_BIT{ BIT0 };
    constexpr EventBits_t SCAN_STOP_BIT{ BIT1 };

    namespace __impl
    {
        struct client
        {
            uint16_t app_id;
            uint16_t gattc_if;
            uint16_t conn_id;
            uint16_t service_start_handle;
            uint16_t service_end_handle;
            esp_bd_addr_t remote_bda;
            esp_ble_addr_type_t addr_type;
            EventGroupHandle_t event_group;
            ble::client::notify_callback_t notify_callback;
            ble::client::disconnect_callback_t disconnect_callback;
            uint16_t* buff_length;
            void* buff;
        };

        static void esp_gap_callback(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
        static void esp_gattc_callback(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);
        static client* const get_client(const esp_gatt_if_t gattc_if);

        static scan_callback_t scan_callback = nullptr;
        static EventGroupHandle_t scan_event_group = nullptr;

        static esp_ble_scan_params_t ble_scan_params{
            BLE_SCAN_TYPE_ACTIVE,           // Scan type
            BLE_ADDR_TYPE_PUBLIC,           // Address type
            BLE_SCAN_FILTER_ALLOW_ALL,      // Filter policy
            0x50,                           // Scan interval
            0x30,                           // Scan window
            BLE_SCAN_DUPLICATE_DISABLE      // Advertise duplicates filter policy
        };

        static std::array<client*, MAX_CLIENTS> gl_profile_tab;

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
                            return;
                        }

                        if (scan_callback == nullptr)
                        {
                            return;
                        }

                        scan_callback(
                            std::string_view(reinterpret_cast<const char*>(adv_name), static_cast<size_t>(adv_name_len)), 
                            mac(param->scan_rst.bda, param->scan_rst.ble_addr_type));
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
            default:
                ESP_LOGW(TAG, "Other GAP event: %x.", event);
                break;
            }
        }

        static void esp_gattc_callback(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param)
        {
            ESP_LOGD(TAG, "Function: %s", __func__);

            esp_err_t result = ESP_OK;

            if (event == ESP_GATTC_REG_EVT)
            {
                if (param->reg.status == ESP_GATT_OK)
                {
                    ESP_LOGI(TAG, "Register app_id: %04x.", param->reg.app_id);

                    client* const ble_client = gl_profile_tab[param->reg.app_id];
                    ble_client->gattc_if = gattc_if;

                    result = esp_ble_gap_set_scan_params(&ble_scan_params);
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

                if (ble_client->notify_callback == nullptr)
                {
                    break;
                }

                ble_client->notify_callback(
                    param->notify.handle, 
                    std::string_view(reinterpret_cast<const char*>(param->notify.value), static_cast<size_t>(param->notify.value_len)));

                break;
            case ESP_GATTC_CONNECT_EVT:
                ESP_LOGV(TAG, "ESP_GATTC_CONNECT_EVT");

                ble_client->conn_id = param->connect.conn_id;
                std::copy(param->connect.remote_bda, param->connect.remote_bda + sizeof(esp_bd_addr_t), ble_client->remote_bda);

                result = esp_ble_gattc_send_mtu_req(gattc_if, param->connect.conn_id);
                if (result != ESP_OK)
                {
                    xEventGroupSetBits(ble_client->event_group, FAIL_BIT);
                    ESP_LOGE(TAG, "MTU configuration failed with error code %x [%s].", result, esp_err_to_name(result));
                    break;
                }
                ESP_LOGI(TAG, "MTU configuration success.");
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

                if (ble_client->disconnect_callback == nullptr)
                {
                    break;
                }

                ble_client->disconnect_callback();

                break;
            case ESP_GATTC_CLOSE_EVT:
                ESP_LOGV(TAG, "ESP_GATTC_CLOSE_EVT");
                break;
            default:
                ESP_LOGW(TAG, "Other GATTC event: %i.", event);
                break;
            }
        }

        static client* const get_client(const esp_gatt_if_t gattc_if)
        {
            ESP_LOGD(TAG, "Function: %s", __func__);
            auto iter = std::find_if(gl_profile_tab.begin(), gl_profile_tab.end(), [gattc_if](const auto& elem) { 
                return (elem != nullptr && elem->gattc_if == gattc_if); 
            });

            return (iter != gl_profile_tab.end()) ? *iter : nullptr;
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

        result = esp_ble_gap_register_callback(&__impl::esp_gap_callback);
        if (result != ESP_OK)
        {
            ESP_LOGE(TAG, "BLE GAP callback register failed with error code %x [%s].", result, esp_err_to_name(result));
            goto cleanup_bluedroid_enable;
        }

        result = esp_ble_gattc_register_callback(&__impl::esp_gattc_callback);
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

        __impl::scan_callback = nullptr;

        __impl::scan_event_group = xEventGroupCreate();
        if (__impl::scan_event_group == nullptr)
        {
            ESP_LOGE(TAG, "Event group create failed.");
            goto cleanup_bluedroid_enable;
        }

        std::for_each(__impl::gl_profile_tab.begin(), __impl::gl_profile_tab.end(), [](auto& elem){
            elem = nullptr;
        });

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

        std::for_each(__impl::gl_profile_tab.begin(), __impl::gl_profile_tab.end(), [](auto& elem) {
            delete elem;
            elem = nullptr;
        });

        __impl::scan_callback = nullptr;
        vEventGroupDelete(__impl::scan_event_group);
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

        EventBits_t bits = xEventGroupWaitBits(__impl::scan_event_group, SCAN_START_BIT | FAIL_BIT, pdTRUE, pdFALSE, BLE_TIMEOUT);

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

        EventBits_t bits = xEventGroupWaitBits(__impl::scan_event_group, SCAN_STOP_BIT | FAIL_BIT, pdTRUE, pdFALSE, BLE_TIMEOUT);

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

    esp_err_t register_scan_callback(scan_callback_t&& callback)
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        __impl::scan_callback = std::move(callback);
        ESP_LOGI(TAG, "Scan callback set.");

        return ESP_OK;
    }

    namespace client
    {
        esp_err_t init(handle_t* client_handle)
        {
            ESP_LOGD(TAG, "Function: %s.", __func__);
            esp_err_t result = ESP_OK;
            *client_handle = INVALID_HANDLE;

            auto iter = std::find_if(__impl::gl_profile_tab.begin(), __impl::gl_profile_tab.end(), [](const auto& elem) {
                return (elem == nullptr);
            });

            if (iter == __impl::gl_profile_tab.end())
            {
                ESP_LOGE(TAG, "Maximum number of devices already connected.");
                result = ESP_FAIL;
                return result;
            }

            {
                uint16_t app_id = std::distance(__impl::gl_profile_tab.begin(), iter);

                /*
                    Provide custom deleter to make sure that *iter is set to nullptr in case
                    something goes wrong.
                */
                std::unique_ptr<__impl::client, std::function<void(__impl::client*)>> ble_client(
                    new __impl::client, 
                    [iter](__impl::client* ptr) {
                        delete ptr;
                        *iter = nullptr;
                    });

                ble_client->app_id = app_id;
                ble_client->gattc_if = ESP_GATT_IF_NONE;
                ble_client->notify_callback = nullptr;
                ble_client->disconnect_callback = nullptr;

                *iter = ble_client.get();

                result = esp_ble_gattc_app_register(ble_client->app_id);
                if (result != ESP_OK)
                {
                    ESP_LOGE(TAG, "Could not register app.");
                    return result;
                }

                ble_client->event_group = xEventGroupCreate();
                if (ble_client->event_group == nullptr)
                {
                    ESP_LOGE(TAG, "Event group create failed.");
                    result = ESP_FAIL;
                    return result;
                }

                *client_handle = app_id;
                ble_client.release(); // Pass full responsibility for memory to client
            }

            ESP_LOGI(TAG, "BLE client initialized.");
            return result;
        }

        esp_err_t destroy(const handle_t client_handle)
        {
            ESP_LOGD(TAG, "Function: %s.", __func__);
            esp_err_t result = ESP_OK;

            __impl::client** ble_client = &__impl::gl_profile_tab[static_cast<size_t>(client_handle)];

            if ((*ble_client) == nullptr)
            {
                ESP_LOGE(TAG, "Invalid client handle.");
                result = ESP_FAIL;
                return result;
            }

            esp_ble_gattc_app_unregister((*ble_client)->gattc_if);

            vEventGroupDelete((*ble_client)->event_group);

            delete (*ble_client);
            (*ble_client) = nullptr;

            ESP_LOGI(TAG, "BLE client destroyed.");
            return result;
        }

        esp_err_t connect(const handle_t client_handle, const mac& address)
        {
            ESP_LOGD(TAG, "Function: %s.", __func__);
            esp_err_t result = ESP_OK;

            __impl::client* ble_client = __impl::gl_profile_tab[static_cast<size_t>(client_handle)];

            result = stop_scanning();
            if (result != ESP_OK)
            {
                ESP_LOGW(TAG, "Scan stop failed with error code %x [%s].", result, esp_err_to_name(result));
            }

            result = esp_ble_gattc_open(
                ble_client->gattc_if, 
                const_cast<uint8_t*>(static_cast<const uint8_t*>(address)), 
                address.get_type(), 
                true);

            if (result != ESP_OK)
            {
                ESP_LOGE(TAG, "GATT connect failed with error code %x [%s].", result, esp_err_to_name(result));
                return result;
            }

            EventBits_t bits = xEventGroupWaitBits(ble_client->event_group, CONNECT_BIT | FAIL_BIT, pdTRUE, pdFALSE, BLE_TIMEOUT);

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

        esp_err_t disconnect(const handle_t client_handle)
        {
            ESP_LOGD(TAG, "Function: %s.", __func__);
            esp_err_t result = ESP_OK;

            __impl::client* ble_client = __impl::gl_profile_tab[static_cast<size_t>(client_handle)];

            result = esp_ble_gattc_close(ble_client->gattc_if, ble_client->conn_id);
            if (result != ESP_OK)
            {
                ESP_LOGE(TAG, "GATT disconnect failed with error code %x [%s].", result, esp_err_to_name(result));
                return result;
            }

            EventBits_t bits = xEventGroupWaitBits(ble_client->event_group, DISCONNECT_BIT | FAIL_BIT, pdTRUE, pdFALSE, BLE_TIMEOUT);

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

        esp_err_t register_for_notify(const handle_t client_handle, uint16_t handle)
        {
            ESP_LOGD(TAG, "Function: %s.", __func__);
            esp_err_t result = ESP_OK;

            __impl::client* ble_client = __impl::gl_profile_tab[static_cast<size_t>(client_handle)];

            result = esp_ble_gattc_register_for_notify(ble_client->gattc_if, ble_client->remote_bda, handle);
            if (result != ESP_OK)
            {
                ESP_LOGE(TAG, "Register for notify failed with error code %x [%s].", result, esp_err_to_name(result));
                return result;
            }

            EventBits_t bits = xEventGroupWaitBits(ble_client->event_group, REG_FOR_NOTIFY_BIT | FAIL_BIT, pdTRUE, pdFALSE, BLE_TIMEOUT);

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

        esp_err_t unregister_for_notify(const handle_t client_handle, uint16_t handle)
        {
            ESP_LOGD(TAG, "Function: %s.", __func__);
            esp_err_t result = ESP_OK;

            __impl::client* ble_client = __impl::gl_profile_tab[static_cast<size_t>(client_handle)];

            result = esp_ble_gattc_unregister_for_notify(ble_client->gattc_if, ble_client->remote_bda, handle);
            if (result != ESP_OK)
            {
                ESP_LOGE(TAG, "Unregister for notify failed with error code %x [%s].", result, esp_err_to_name(result));
                return result;
            }

            EventBits_t bits = xEventGroupWaitBits(ble_client->event_group, UNREG_FOR_NOTIFY_BIT | FAIL_BIT, pdTRUE, pdFALSE, BLE_TIMEOUT);

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

        esp_err_t register_notify_callback(const handle_t client_handle, notify_callback_t&& callback)
        {
            ESP_LOGD(TAG, "Function: %s.", __func__);
            __impl::client* ble_client = __impl::gl_profile_tab[static_cast<size_t>(client_handle)];
            ble_client->notify_callback = std::move(callback);
            return ESP_OK;
        }

        esp_err_t register_disconnect_callback(const handle_t client_handle, disconnect_callback_t callback)
        {
            ESP_LOGD(TAG, "Function: %s.", __func__);

            __impl::client* ble_client = __impl::gl_profile_tab[static_cast<size_t>(client_handle)];
            ble_client->disconnect_callback = callback;
            return ESP_OK;
        }

        esp_err_t get_services(const handle_t client_handle, esp_gattc_service_elem_t* services, uint16_t* count)
        {
            ESP_LOGD(TAG, "Function: %s.", __func__);
            esp_err_t result = ESP_OK;

            __impl::client* ble_client = __impl::gl_profile_tab[static_cast<size_t>(client_handle)];

            ESP_LOGW(TAG, "Function not implemented.");

            return result;
        }

        esp_err_t get_service(const handle_t client_handle, const esp_bt_uuid_t* uuid)
        {
            ESP_LOGD(TAG, "Function: %s.", __func__);
            esp_err_t result = ESP_OK;

            __impl::client* ble_client = __impl::gl_profile_tab[static_cast<size_t>(client_handle)];

            result = esp_ble_gattc_search_service(ble_client->gattc_if, ble_client->conn_id, const_cast<esp_bt_uuid_t*>(uuid));
            if (result != ESP_OK)
            {
                ESP_LOGE(TAG, "Get service failed with error code %x [%s].", result, esp_err_to_name(result));
                return result;
            }

            EventBits_t bits = xEventGroupWaitBits(ble_client->event_group, SEARCH_SERVICE_BIT | FAIL_BIT, pdTRUE, pdFALSE, BLE_TIMEOUT);

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

        esp_err_t get_characteristics(const handle_t client_handle, esp_gattc_char_elem_t* characteristics, uint16_t* count)
        {
            ESP_LOGD(TAG, "Function: %s.", __func__);
            esp_gatt_status_t result = ESP_GATT_OK;

            __impl::client* ble_client = __impl::gl_profile_tab[static_cast<size_t>(client_handle)];

            if (characteristics != nullptr)
            {
                result = esp_ble_gattc_get_all_char(
                    ble_client->gattc_if, 
                    ble_client->conn_id,
                    ble_client->service_start_handle,
                    ble_client->service_end_handle,
                    characteristics,
                    count,
                    0);
            }
            else
            {
                result = esp_ble_gattc_get_attr_count(
                    ble_client->gattc_if, 
                    ble_client->conn_id,
                    ESP_GATT_DB_CHARACTERISTIC,
                    ble_client->service_start_handle,
                    ble_client->service_end_handle,
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

        esp_err_t write_characteristic(const handle_t client_handle, const uint16_t handle, const uint8_t* value, const uint16_t value_length)
        {
            ESP_LOGD(TAG, "Function: %s.", __func__);
            esp_err_t result = ESP_OK;

            __impl::client* ble_client = __impl::gl_profile_tab[static_cast<size_t>(client_handle)];

            result = esp_ble_gattc_write_char(
                ble_client->gattc_if, 
                ble_client->conn_id,
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

            EventBits_t bits = xEventGroupWaitBits(ble_client->event_group, WRITE_CHAR_BIT | FAIL_BIT, pdTRUE, pdFALSE, BLE_TIMEOUT);

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

        esp_err_t read_characteristic(const handle_t client_handle, const uint16_t handle, uint8_t* value, uint16_t* value_length)
        {
            ESP_LOGD(TAG, "Function: %s.", __func__);
            esp_err_t result = ESP_OK;

            __impl::client* ble_client = __impl::gl_profile_tab[static_cast<size_t>(client_handle)];

            ble_client->buff = value;
            ble_client->buff_length = value_length;

            result = esp_ble_gattc_read_char(
                ble_client->gattc_if, 
                ble_client->conn_id,
                handle,
                ESP_GATT_AUTH_REQ_NONE
            );

            if (result != ESP_OK)
            {
                ESP_LOGE(TAG, "Read characteristic failed with error code %x [%s].", result, esp_err_to_name(result));
                return result;
            }

            EventBits_t bits = xEventGroupWaitBits(ble_client->event_group, READ_CHAR_BIT | FAIL_BIT, pdTRUE, pdFALSE, BLE_TIMEOUT);

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

            ble_client->buff = nullptr;
            ble_client->buff_length = nullptr;

            return result;
        }

        esp_err_t get_descriptors(const handle_t client_handle, const uint16_t char_handle, esp_gattc_descr_elem_t* descr, uint16_t* count)
        {
            ESP_LOGD(TAG, "Function: %s.", __func__);
            esp_gatt_status_t result = ESP_GATT_OK;

            __impl::client* ble_client = __impl::gl_profile_tab[static_cast<size_t>(client_handle)];

            if (descr != nullptr)
            {
                result = esp_ble_gattc_get_all_descr(
                    ble_client->gattc_if, 
                    ble_client->conn_id,
                    char_handle,
                    descr,
                    count,
                    0);
            }
            else
            {
                result = esp_ble_gattc_get_attr_count(
                    ble_client->gattc_if, 
                    ble_client->conn_id,
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

        esp_err_t write_descriptor(const handle_t client_handle, const uint16_t handle, const uint8_t* value, const uint16_t value_length)
        {
            ESP_LOGD(TAG, "Function: %s.", __func__);
            esp_err_t result = ESP_OK;

            __impl::client* ble_client = __impl::gl_profile_tab[static_cast<size_t>(client_handle)];

            result = esp_ble_gattc_write_char_descr(
                ble_client->gattc_if, 
                ble_client->conn_id,
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

            EventBits_t bits = xEventGroupWaitBits(ble_client->event_group, WRITE_DESCR_BIT | FAIL_BIT, pdTRUE, pdFALSE, BLE_TIMEOUT);

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

        esp_err_t read_descriptor(const handle_t client_handle, const uint16_t handle, uint8_t* value, uint16_t* value_length)
        {
            ESP_LOGD(TAG, "Function: %s.", __func__);
            esp_err_t result = ESP_OK;

            __impl::client* ble_client = __impl::gl_profile_tab[static_cast<size_t>(client_handle)];

            ble_client->buff = value;
            ble_client->buff_length = value_length;

            result = esp_ble_gattc_read_char_descr(
                ble_client->gattc_if, 
                ble_client->conn_id,
                (uint16_t)handle,
                ESP_GATT_AUTH_REQ_NONE
            );

            if (result != ESP_OK)
            {
                ESP_LOGE(TAG, "Read descriptor failed with error code %x [%s].", result, esp_err_to_name(result));
                return result;
            }

            EventBits_t bits = xEventGroupWaitBits(ble_client->event_group, READ_DESCR_BIT | FAIL_BIT, pdTRUE, pdFALSE, BLE_TIMEOUT);

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

            ble_client->buff = nullptr;
            ble_client->buff_length = nullptr;

            return result;
        }
    }
}