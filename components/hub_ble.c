#include "hub_ble.h"

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gatt_defs.h"
#include "esp_gatt_common_api.h"
#include "esp_gap_ble_api.h"
#include "esp_log.h"

#define BLE_TIMEOUT (TickType_t)10000 / portTICK_PERIOD_MS

#define CONNECT_BIT             BIT0
#define SEARCH_SERVICE_BIT      BIT1
#define WRITE_CHAR_BIT          BIT2
#define READ_CHAR_BIT           BIT3
#define WRITE_DESCR_BIT         BIT4
#define READ_DESCR_BIT          BIT5
#define REG_FOR_NOTIFY_BIT      BIT6
#define UNREG_FOR_NOTIFY_BIT    BIT7
#define DISCONNECT_BIT          BIT8
#define FAIL_BIT                BIT15

static const char* TAG = "HUB_BLE";

static void esp_gap_callback(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
static void esp_gattc_callback(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);
static hub_ble_client* get_client(esp_gatt_if_t gattc_if);

static scan_callback_t scan_callback;

static esp_ble_scan_params_t ble_scan_params = {
    .scan_type = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval = 0x50,
    .scan_window = 0x30
};

static hub_ble_client* gl_profile_tab[HUB_BLE_MAX_CLIENTS];

static void esp_gap_callback(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    ESP_LOGD(TAG, "Function: %s.", __func__);

    uint8_t *adv_name = NULL;
    uint8_t adv_name_len = 0;

    switch (event)
    {
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT:
        ESP_LOGV(TAG, "ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT");
        ESP_LOGI(TAG, "Set scan parameters complete.");
        break;
    case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
        ESP_LOGV(TAG, "ESP_GAP_BLE_SCAN_START_COMPLETE_EVT");
        if (param->scan_start_cmpl.status != ESP_BT_STATUS_SUCCESS)
        {
            ESP_LOGE(TAG, "Scan start failed with error code %i.", param->scan_start_cmpl.status);
            break;
        }
        ESP_LOGI(TAG, "Scan started...");
        break;
    case ESP_GAP_BLE_SCAN_RESULT_EVT:
        ESP_LOGV(TAG, "ESP_GAP_BLE_SCAN_RESULT_EVT");
        switch (param->scan_rst.search_evt)
        {
        case ESP_GAP_SEARCH_INQ_RES_EVT:
            ESP_LOGV(TAG, "ESP_GAP_SEARCH_INQ_RES_EVT");
            adv_name = esp_ble_resolve_adv_data(param->scan_rst.ble_adv, ESP_BLE_AD_TYPE_NAME_CMPL, &adv_name_len);

            if (adv_name != NULL && strlen((const char*)adv_name) != 0)
            {
                ESP_LOGI(TAG, "Found device %s.", adv_name); 
                if (scan_callback != NULL)
                {
                    scan_callback(param->scan_rst.bda, (const char*)adv_name, param->scan_rst.ble_addr_type);
                }
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
            ESP_LOGE(TAG, "Scan stop failed failed with error code %i.", param->scan_stop_cmpl.status);
            break;
        }

        ESP_LOGI(TAG, "Scan stopped successfully");
        break;
    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
        ESP_LOGV(TAG, "ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT");
    default:
        ESP_LOGW(TAG, "Other GAP event: %i.", event);
        break;
    }
}

static void esp_gattc_callback(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param)
{
    ESP_LOGD(TAG, "Function: %s", __func__);

    esp_err_t result = ESP_OK;
    hub_ble_client* client = NULL;

    switch (event)
    {
    case ESP_GATTC_REG_EVT:
        ESP_LOGV(TAG, "ESP_GATTC_REG_EVT");

        client = gl_profile_tab[param->reg.app_id];

        if (param->reg.status == ESP_GATT_OK)
        {
            client->gattc_if = gattc_if;
            client->app_id = param->reg.app_id;

            ESP_LOGI(TAG, "Register app success, app_id %04x.", client->app_id);

            result = esp_ble_gap_set_scan_params(&ble_scan_params);
            if (result != ESP_OK)
            {
                ESP_LOGE(TAG, "Set scan parameters failed with error code %x [%s].", result, esp_err_to_name(result));
            }
        }
        else
        {
            ESP_LOGE(TAG, "Register app failed, app_id %04x, status %d", param->reg.app_id, param->reg.status);
            return;
        }

        break;
    case ESP_GATTC_CONNECT_EVT:
        ESP_LOGV(TAG, "ESP_GATTC_CONNECT_EVT");

        client = get_client(gattc_if);
        if (client == NULL)
        {
            ESP_LOGE(TAG, "Client not found.");
            break;
        }

        client->conn_id = param->connect.conn_id;
        memcpy(client->remote_bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));

        result = esp_ble_gattc_send_mtu_req(gattc_if, param->connect.conn_id);
        if (result != ESP_OK)
        {
            xEventGroupSetBits(client->event_group, FAIL_BIT);
            ESP_LOGE(TAG, "MTU configuration failed, error: %x", result);
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

        client = get_client(gattc_if);
        if (client == NULL)
        {
            ESP_LOGE(TAG, "Client not found.");
            break;
        }

        xEventGroupSetBits(client->event_group, CONNECT_BIT);
        break;
    case ESP_GATTC_SEARCH_RES_EVT:
        ESP_LOGV(TAG, "ESP_GATTC_SEARCH_RES_EVT");

        client = get_client(gattc_if);
        if (client == NULL)
        {
            ESP_LOGE(TAG, "Client not found.");
            xEventGroupSetBits(client->event_group, FAIL_BIT);
            break;
        }

        client->service_start_handle = param->search_res.start_handle;
        client->service_end_handle = param->search_res.end_handle;
        break;
    case ESP_GATTC_SEARCH_CMPL_EVT:
        ESP_LOGV(TAG, "ESP_GATTC_SEARCH_CMPL_EVT");

        client = get_client(gattc_if);
        if (client == NULL)
        {
            ESP_LOGE(TAG, "Client not found.");
            break;
        }

        xEventGroupSetBits(client->event_group, SEARCH_SERVICE_BIT);
        break;
    case ESP_GATTC_REG_FOR_NOTIFY_EVT:
        ESP_LOGV(TAG, "ESP_GATTC_REG_FOR_NOTIFY_EVT");

        client = get_client(gattc_if);
        if (client == NULL)
        {
            ESP_LOGE(TAG, "Client not found.");
            break;
        }

        xEventGroupSetBits(client->event_group, REG_FOR_NOTIFY_BIT);
        break;
    case ESP_GATTC_UNREG_FOR_NOTIFY_EVT:
        ESP_LOGV(TAG, "ESP_GATTC_UNREG_FOR_NOTIFY_EVT");

        client = get_client(gattc_if);
        if (client == NULL)
        {
            ESP_LOGE(TAG, "Client not found.");
            break;
        }

        xEventGroupSetBits(client->event_group, UNREG_FOR_NOTIFY_BIT);
        break;
    case ESP_GATTC_NOTIFY_EVT:
        ESP_LOGV(TAG, "ESP_GATTC_NOTIFY_EVT");

        client = get_client(gattc_if);
        if (client == NULL)
        {
            ESP_LOGE(TAG, "Client not found.");
            break;
        }

        if (client->notify_cb == NULL)
        {
            break;
        }

        client->notify_cb(client, param->notify.handle, param->notify.value, param->notify.value_len);

        break;
    case ESP_GATTC_WRITE_CHAR_EVT:
        ESP_LOGV(TAG, "ESP_GATTC_WRITE_CHAR_EVT");

        client = get_client(gattc_if);
        if (client == NULL)
        {
            ESP_LOGE(TAG, "Client not found.");
            break;
        }

        xEventGroupSetBits(client->event_group, WRITE_CHAR_BIT);
        break;
    case ESP_GATTC_READ_CHAR_EVT:
        ESP_LOGV(TAG, "ESP_GATTC_READ_CHAR_EVT");

        client = get_client(gattc_if);
        if (client == NULL)
        {
            ESP_LOGE(TAG, "Client not found.");
            break;
        }

        if (client->_buff_length == NULL)
        {
            ESP_LOGW(TAG, "Buffer length pointer is NULL.");
            xEventGroupSetBits(client->event_group, READ_CHAR_BIT);
            break;
        }

        // If the length of the buffer is not set by the previous call, we only provide a required buffer length
        if (*(client->_buff_length) != param->read.value_len)
        {
            *(client->_buff_length) = param->read.value_len;
            xEventGroupSetBits(client->event_group, READ_CHAR_BIT);
            break;
        }

        if (client->_buff == NULL)
        {
            ESP_LOGW(TAG, "Buffer not pointer is NULL.");
            xEventGroupSetBits(client->event_group, READ_CHAR_BIT);
            break;
        }

        // If the buffer is prepared correctly and the length matches, we copy the content of the characteristic
        memcpy(client->_buff, param->read.value, client->_buff_length);

        xEventGroupSetBits(client->event_group, READ_CHAR_BIT);
        break;
    case ESP_GATTC_WRITE_DESCR_EVT:
        ESP_LOGV(TAG, "ESP_GATTC_WRITE_DESCR_EVT");

        client = get_client(gattc_if);
        if (client == NULL)
        {
            ESP_LOGE(TAG, "Client not found.");
            break;
        }

        xEventGroupSetBits(client->event_group, WRITE_DESCR_BIT);
        break;
    case ESP_GATTC_READ_DESCR_EVT:
        ESP_LOGV(TAG, "ESP_GATTC_READ_DESCR_EVT");

        client = get_client(gattc_if);
        if (client == NULL)
        {
            ESP_LOGE(TAG, "Client not found.");
            break;
        }

        if (client->_buff_length == NULL)
        {
            ESP_LOGW(TAG, "Buffer length pointer is NULL.");
            xEventGroupSetBits(client->event_group, READ_DESCR_BIT);
            break;
        }

        // If the length of the buffer is not set by the previous call, we only provide a required buffer length
        if (*(client->_buff_length) != param->read.value_len)
        {
            *(client->_buff_length) = param->read.value_len;
            xEventGroupSetBits(client->event_group, READ_DESCR_BIT);
            break;
        }

        if (client->_buff == NULL)
        {
            ESP_LOGW(TAG, "Buffer pointer is NULL.");
            xEventGroupSetBits(client->event_group, READ_DESCR_BIT);
            break;
        }

        // If the buffer is prepared correctly and the length matches, we copy the content of the descriptor
        memcpy(client->_buff, param->read.value, client->_buff_length);

        xEventGroupSetBits(client->event_group, READ_DESCR_BIT);
        break;
    case ESP_GATTC_SRVC_CHG_EVT:
        ESP_LOGV(TAG, "ESP_GATTC_SRVC_CHG_EVT");
        break;
    case ESP_GATTC_DISCONNECT_EVT:
        ESP_LOGV(TAG, "ESP_GATTC_DISCONNECT_EVT");

        client = get_client(gattc_if);
        if (client == NULL)
        {
            ESP_LOGE(TAG, "Client not found.");
            break;
        }

        xEventGroupSetBits(client->event_group, DISCONNECT_BIT);

        if (client->disconnect_cb == NULL)
        {
            break;
        }

        client->disconnect_cb(client);
        break;
    case ESP_GATTC_CLOSE_EVT:
        ESP_LOGV(TAG, "ESP_GATTC_CLOSE_EVT");
        break;
    default:
        ESP_LOGW(TAG, "Other GATTC event: %i.", event);
        break;
    }
}

static hub_ble_client* get_client(esp_gatt_if_t gattc_if)
{
    ESP_LOGD(TAG, "Function: %s", __func__);
    
    for (int i = 0; i < HUB_BLE_MAX_CLIENTS; i++)
    {
        if (gl_profile_tab[i] != NULL && gl_profile_tab[i]->gattc_if != ESP_GATT_IF_NONE && gl_profile_tab[i]->gattc_if == gattc_if)
        {
            return gl_profile_tab[i];
        }
    }

    return NULL;
}

esp_err_t hub_ble_init()
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

    result = esp_ble_gattc_register_callback(&esp_gattc_callback);
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

    scan_callback = NULL;

    for (uint8_t i = 0; i < HUB_BLE_MAX_CLIENTS; i++)
    {
        gl_profile_tab[i] = NULL;
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

esp_err_t hub_ble_deinit()
{
    ESP_LOGD(TAG, "Function: %s.", __func__);

    for (uint8_t i = 0; i < HUB_BLE_MAX_CLIENTS; i++)
    {
        gl_profile_tab[i] = NULL;
    }

    scan_callback = NULL;

    esp_bluedroid_disable();
    esp_bluedroid_deinit();
    esp_bt_controller_disable();
    esp_bt_controller_deinit();

    return ESP_OK;
}

esp_err_t hub_ble_start_scanning(uint32_t scan_time)
{
    ESP_LOGD(TAG, "Function: %s.", __func__);
    return  esp_ble_gap_start_scanning(scan_time);
}

esp_err_t hub_ble_stop_scanning()
{
    ESP_LOGD(TAG, "Function: %s.", __func__);
    return esp_ble_gap_stop_scanning();
}

esp_err_t hub_ble_register_scan_callback(scan_callback_t callback)
{
    ESP_LOGD(TAG, "Function: %s.", __func__);

    scan_callback = callback;
    ESP_LOGI(TAG, "Register callback set.");

    return ESP_OK;
}

esp_err_t hub_ble_client_init(hub_ble_client* ble_client)
{
    ESP_LOGD(TAG, "Function: %s.", __func__);
    esp_err_t result = ESP_OK;
    uint8_t app_id = 0;

    ble_client->gattc_if = ESP_GATT_IF_NONE;
    ble_client->notify_cb = NULL;
    ble_client->disconnect_cb = NULL;

    while (app_id != HUB_BLE_MAX_CLIENTS)
    {
        if (gl_profile_tab[app_id] == NULL)
        {
            gl_profile_tab[app_id] = ble_client;
            break;
        }
        app_id++;
    }

    if (app_id == HUB_BLE_MAX_CLIENTS)
    {
        ESP_LOGE(TAG, "Maximum number of devices already connected.");
        return ESP_FAIL;
    }

    result = esp_ble_gattc_app_register(app_id);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "BLE GATC app register failed with error code %x [%s].", result, esp_err_to_name(result));
        gl_profile_tab[app_id] = NULL;
        return result;
    }

    ble_client->event_group = xEventGroupCreate();
    if (ble_client->event_group == NULL)
    {
        ESP_LOGE(TAG, "Event group create failed.");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "BLE client initialized.");
    return result;
}

esp_err_t hub_ble_client_destroy(hub_ble_client* ble_client)
{
    ESP_LOGD(TAG, "Function: %s.", __func__);
    esp_err_t result = ESP_OK;

    vEventGroupDelete(ble_client->event_group);

    result = esp_ble_gattc_app_unregister(ble_client->gattc_if);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "BLE GATC app register failed with error code %x [%s].", result, esp_err_to_name(result));
        return result;
    }

    gl_profile_tab[ble_client->app_id] = NULL;
    ble_client->gattc_if = ESP_GATT_IF_NONE;
    ble_client->notify_cb = NULL;
    ble_client->disconnect_cb = NULL;

    ESP_LOGI(TAG, "BLE client destroyed.");
    return ESP_OK;
}

esp_err_t hub_ble_client_connect(hub_ble_client* ble_client)
{
    ESP_LOGD(TAG, "Function: %s.", __func__);
    esp_err_t result = ESP_OK;

    result = esp_ble_gap_stop_scanning();
    if (result != ESP_OK)
    {
        ESP_LOGW(TAG, "Scan stop failed with error code %x [%s].", result, esp_err_to_name(result));
        //return result;
    }

    result = esp_ble_gattc_open(ble_client->gattc_if, ble_client->remote_bda, ble_client->addr_type, true);
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

esp_err_t hub_ble_client_disconnect(hub_ble_client* ble_client)
{
    ESP_LOGD(TAG, "Function: %s.", __func__);
    esp_err_t result = ESP_OK;

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

esp_err_t hub_ble_client_register_for_notify(hub_ble_client* ble_client, uint16_t handle)
{
    ESP_LOGD(TAG, "Function: %s.", __func__);
    esp_err_t result = ESP_OK;

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

esp_err_t hub_ble_client_unregister_for_notify(hub_ble_client* ble_client, uint16_t handle)
{
    ESP_LOGD(TAG, "Function: %s.", __func__);
    esp_err_t result = ESP_OK;

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

esp_err_t hub_ble_client_register_notify_callback(hub_ble_client* ble_client, notify_callback_t callback)
{
    ESP_LOGD(TAG, "Function: %s.", __func__);

    if (callback == NULL)
    {
        ESP_LOGE(TAG, "Callback is NULL.");
        return ESP_FAIL;
    }

    ble_client->notify_cb = callback;
    return ESP_OK;
}

esp_err_t hub_ble_client_register_disconnect_callback(hub_ble_client* ble_client, disconnect_callback_t callback)
{
    ESP_LOGD(TAG, "Function: %s.", __func__);

    if (callback == NULL)
    {
        ESP_LOGE(TAG, "Callback is NULL.");
        return ESP_FAIL;
    }

    ble_client->disconnect_cb = callback;
    return ESP_OK;
}

esp_err_t hub_ble_client_get_services(hub_ble_client* ble_client, esp_gattc_service_elem_t* services, uint16_t* count)
{
    ESP_LOGD(TAG, "Function: %s.", __func__);
    esp_err_t result = ESP_OK;



    return result;
}

esp_err_t hub_ble_client_get_service(hub_ble_client* ble_client, esp_bt_uuid_t* uuid)
{
    ESP_LOGD(TAG, "Function: %s.", __func__);
    esp_err_t result = ESP_OK;

    result = esp_ble_gattc_search_service(ble_client->gattc_if, ble_client->conn_id, uuid);
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

esp_gatt_status_t hub_ble_client_get_characteristics(hub_ble_client* ble_client, esp_gattc_char_elem_t* characteristics, uint16_t* count)
{
    ESP_LOGD(TAG, "Function: %s.", __func__);
    esp_gatt_status_t result = ESP_GATT_OK;

    if (characteristics != NULL)
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
    }

    return result;
}

esp_err_t hub_ble_client_write_characteristic(hub_ble_client* ble_client, uint16_t handle, uint8_t* value, uint16_t value_length)
{
    ESP_LOGD(TAG, "Function: %s.", __func__);
    esp_err_t result = ESP_OK;

    result = esp_ble_gattc_write_char(
        ble_client->gattc_if, 
        ble_client->conn_id,
        handle,
        value_length,
        value,
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

esp_err_t hub_ble_client_read_characteristic(hub_ble_client* ble_client, uint16_t handle, uint8_t* value, uint16_t* value_length)
{
    ESP_LOGD(TAG, "Function: %s.", __func__);
    esp_err_t result = ESP_OK;

    ble_client->_buff = value;
    ble_client->_buff_length = value_length;

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

    ble_client->_buff = NULL;
    ble_client->_buff_length = NULL;

    return result;
}

esp_gatt_status_t hub_ble_client_get_descriptors(hub_ble_client* ble_client, uint16_t char_handle, esp_gattc_descr_elem_t* descr, uint16_t* count)
{
    ESP_LOGD(TAG, "Function: %s.", __func__);
    esp_gatt_status_t result = ESP_GATT_OK;

    if (descr != NULL)
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
        ESP_LOGE(TAG, "Get descriptors failed with error code %i.", result);
    }

    return result;
}

esp_err_t hub_ble_client_write_descriptor(hub_ble_client* ble_client, uint16_t handle, uint8_t* value, uint16_t value_length)
{
    ESP_LOGD(TAG, "Function: %s.", __func__);
    esp_err_t result = ESP_OK;

    result = esp_ble_gattc_write_char_descr(
        ble_client->gattc_if, 
        ble_client->conn_id,
        handle,
        value_length,
        value,
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

esp_err_t hub_ble_client_read_descriptor(hub_ble_client* ble_client, uint16_t handle, uint8_t* value, uint16_t* value_length)
{
    ESP_LOGD(TAG, "Function: %s.", __func__);
    esp_err_t result = ESP_OK;

    ble_client->_buff = value;
    ble_client->_buff_length = value_length;

    result = esp_ble_gattc_read_char_descr(
        ble_client->gattc_if, 
        ble_client->conn_id,
        handle,
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

    ble_client->_buff = NULL;
    ble_client->_buff_length = NULL;

    return result;
}