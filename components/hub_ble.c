#include "hub_ble.h"

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

#include "esp_bt.h"
#include "esp_gatt_defs.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#define WRITE_CHAR_BIT  BIT0
#define READ_CHAR_BIT   BIT1
#define WRITE_DESCR_BIT BIT2
#define READ_DESCR_BIT  BIT3
#define FAIL_BIT        BIT7

static void esp_gap_callback(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
static void esp_gattc_callback(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);

/* GLOBAL IMPLEMENTATIONS */

esp_ble_scan_params_t ble_scan_params = {
    .scan_type = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval = 0x50,
    .scan_window = 0x30
};

gattc_profile_t gl_profile_tab[PROFILE_NUM];

/* END OF GLOBAL IMPLEMENTATIONS */

static const char* TAG = "HUB_BLE";

static int registered_apps;
static scan_callback_t scan_callback;
static EventGroupHandle_t ble_event_group;

static void esp_gap_callback(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    uint8_t *adv_name = NULL;
    uint8_t adv_name_len = 0;

    switch (event)
    {
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT:
        ESP_LOGI(TAG, "Set scan parameters complete.");
        esp_ble_gap_start_scanning(30U /* seconds */);
        break;
    case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
        //scan start complete event to indicate scan start successfully or failed
        if (param->scan_start_cmpl.status != ESP_BT_STATUS_SUCCESS)
        {
            ESP_LOGE(TAG, "Scan start failed, error: %x", param->scan_start_cmpl.status);
            break;
        }
        ESP_LOGI(TAG, "Scan started...");
        break;
    case ESP_GAP_BLE_SCAN_RESULT_EVT:
        ESP_LOGI(TAG, "Scan complete!");      
        switch (param->scan_rst.search_evt)
        {
        case ESP_GAP_SEARCH_INQ_RES_EVT:
            adv_name = esp_ble_resolve_adv_data(param->scan_rst.ble_adv, ESP_BLE_AD_TYPE_NAME_CMPL, &adv_name_len);

            if (adv_name != NULL && strlen((const char*)adv_name) != 0)
            {
                if (scan_callback != NULL)
                {
                    ESP_LOGI(TAG, "Calling scan callback...");
                    scan_callback(param->scan_rst.bda, (const char*)adv_name);
                }

                if (strncmp((char*)adv_name, "MiKettle", adv_name_len) == 0)
                {
                    ESP_LOGI(TAG, "Device found!");
                    esp_ble_gap_stop_scanning();
                    esp_ble_gattc_open(gl_profile_tab[0].gattc_if, param->scan_rst.bda, param->scan_rst.ble_addr_type, true);
                }
            }
            break;
        case ESP_GAP_SEARCH_INQ_CMPL_EVT:
            break;
        default: break;
        }
        break;
    case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
        if (param->scan_stop_cmpl.status != ESP_BT_STATUS_SUCCESS)
        {
            ESP_LOGE(TAG, "Scan stop failed, wrror: %x", param->scan_stop_cmpl.status);
            break;
        }
        ESP_LOGI(TAG, "Scan stopped successfully");
        break;
    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
        ESP_LOGI(TAG, "Update connection params:\nstatus = %d,\nmin_int = %d,\nmax_int = %d,\nconn_int = %d,\nlatency = %d,\ntimeout = %d",
            param->update_conn_params.status,
            param->update_conn_params.min_int,
            param->update_conn_params.max_int,
            param->update_conn_params.conn_int,
            param->update_conn_params.latency,
            param->update_conn_params.timeout);
        break;
    default: break;
    }
}

static void esp_gattc_callback(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param)
{
    esp_err_t result = ESP_OK;
    gattc_profile_t* client = &gl_profile_tab[param->reg.app_id];
    client->_last_event = event;
    client->_data = *param;

    switch (event)
    {
    case ESP_GATTC_REG_EVT:
        ESP_LOGI(TAG, "ESP_GATTC_REG_EVT");

        if (param->reg.status == ESP_GATT_OK)
        {
            client->gattc_if = gattc_if;
            client->app_id = param->reg.app_id;

            esp_err_t result = esp_ble_gap_set_scan_params(&ble_scan_params);
            if (result != ESP_OK)
            {
                ESP_LOGE(TAG, "Set scan parameters failed in function %s with error code %x.\n", __func__, result);
            }
        }
        else
        {
            ESP_LOGI(TAG, "Register app failed, app_id %04x, status %d", param->reg.app_id, param->reg.status);
            return;
        }

        break;
    case ESP_GATTC_CONNECT_EVT:
        ESP_LOGI(TAG, "ESP_GATTC_CONNECT_EVT");

        client->conn_id = param->connect.conn_id;
        memcpy(client->remote_bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));

        result = esp_ble_gattc_send_mtu_req(gattc_if, param->connect.conn_id);
        if (result != ESP_OK)
        {
            ESP_LOGE(TAG, "MTU configuration failed, error: %x", result);
            break;
        }
        break;
    case ESP_GATTC_OPEN_EVT:
        ESP_LOGI(TAG, "ESP_GATTC_OPEN_EVT");
        break;
    case ESP_GATTC_DIS_SRVC_CMPL_EVT:
        ESP_LOGI(TAG, "ESP_GATTC_DIS_SRVC_CMPL_EVT");
        break;
    case ESP_GATTC_CFG_MTU_EVT:
        ESP_LOGI(TAG, "ESP_GATTC_CFG_MTU_EVT");
        break;
    case ESP_GATTC_SEARCH_RES_EVT:
        ESP_LOGI(TAG, "ESP_GATTC_SEARCH_RES_EVT");
        break;
    case ESP_GATTC_SEARCH_CMPL_EVT:
        ESP_LOGI(TAG, "ESP_GATTC_SEARCH_CMPL_EVT");
        break;
    case ESP_GATTC_REG_FOR_NOTIFY_EVT:
        ESP_LOGI(TAG, "ESP_GATTC_REG_FOR_NOTIFY_EVT");
        break;
    case ESP_GATTC_NOTIFY_EVT:
        ESP_LOGI(TAG, "ESP_GATTC_NOTIFY_EVT");
        break;
    case ESP_GATTC_READ_CHAR_EVT:
        ESP_LOGI(TAG, "ESP_GATTC_READ_CHAR_EVT");
        break;
    case ESP_GATTC_WRITE_DESCR_EVT:
        ESP_LOGI(TAG, "ESP_GATTC_WRITE_DESCR_EVT");
        break;
    case ESP_GATTC_SRVC_CHG_EVT:
        ESP_LOGI(TAG, "ESP_GATTC_SRVC_CHG_EVT");
        break;
    case ESP_GATTC_WRITE_CHAR_EVT:
        ESP_LOGI(TAG, "ESP_GATTC_WRITE_CHAR_EVT");
        xEventGroupSetBits(ble_event_group, WRITE_CHAR_BIT);
        break;
    case ESP_GATTC_DISCONNECT_EVT:
        ESP_LOGI(TAG, "ESP_GATTC_DISCONNECT_EVT");
        break;
    default: break;
    }
}

esp_err_t hub_ble_init()
{
    esp_err_t result = ESP_OK;

    esp_bt_controller_config_t bt_config = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    result = esp_bt_controller_init(&bt_config);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "BLE initialization failed in function %s with error code %x.\n", __func__, result);
        return result;
    }

    result = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "BLE controller enable failed in function %s with error code %x.\n", __func__, result);
        return result;
    }

    result = esp_bluedroid_init();
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Bluedroid initialization failed in function %s with error code %x.\n", __func__, result);
        return result;
    }

    result = esp_bluedroid_enable();
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Bluedroid enable failed in function %s with error code %x.\n", __func__, result);
        return result;
    }

    result = esp_ble_gap_register_callback(esp_gap_callback);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "BLE GAP callback register failed in function %s with error code %x.\n", __func__, result);
        return result;
    }

    ble_event_group = xEventGroupCreate();
    if (ble_event_group == NULL)
    {
        ESP_LOGE(TAG, "Could not create event group.\n");
        return ESP_FAIL;
    }

    result = esp_ble_gattc_register_callback(esp_gattc_callback);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "BLE GATC callback register failed in function %s with error code %x.\n", __func__, result);
        return result;
    }

    result = esp_ble_gatt_set_local_mtu(500);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "BLE GATT set local MTU failed in function %s with error code %x.\n", __func__, result);
        return result;
    }

    registered_apps = 0;

    return result;
}

esp_err_t hub_ble_deinit()
{
    vEventGroupDelete(ble_event_group);
    return ESP_OK;
}

esp_err_t hub_ble_register_scan_callback(scan_callback_t callback)
{
    if (callback == NULL)
    {
        return ESP_FAIL;
    }

    scan_callback = callback;
    return ESP_OK;
}

esp_err_t hub_ble_client_init(gattc_profile_t* ble_client)
{
    esp_err_t result = ESP_OK;

    ble_client->gattc_if = ESP_GATT_IF_NONE;
    result = esp_ble_gattc_app_register(registered_apps);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "BLE GATC app register failed in function %s with error code %x.\n", __func__, result);
        return result;
    }

    registered_apps++;

    return result;
}

esp_err_t hub_ble_client_destroy(gattc_profile_t* ble_client)
{
    esp_err_t result = ESP_OK;

    return result;
}

esp_err_t hub_ble_client_connect(gattc_profile_t* ble_client)
{
    esp_err_t result = ESP_OK;

    return result;
}

esp_err_t hub_ble_client_search_service(gattc_profile_t* ble_client)
{
    esp_err_t result = ESP_OK;

    return result;
}

esp_err_t hub_ble_client_write_characteristic(gattc_profile_t* ble_client, uint16_t handle, uint8_t* value, uint16_t value_length)
{
    esp_err_t result = ESP_OK;

    result = esp_ble_gattc_write_char(
        ble_client->gattc_if, 
        ble_client->conn_id,
        handle,
        value_length,
        value,
        ESP_GATT_WRITE_TYPE_NO_RSP,
        ESP_GATT_AUTH_REQ_NONE);

    EventBits_t bits = xEventGroupWaitBits(ble_event_group, WRITE_CHAR_BIT, pdTRUE, pdFALSE, 5000 / portTICK_PERIOD_MS);

    if (bits & WRITE_CHAR_BIT)
    {
        ESP_LOGI(TAG, "Write characteristic success.");
    }
    else
    {
        ESP_LOGE(TAG, "Write characteristic error.");
    }

    return result;
}

esp_err_t hub_ble_client_read_characteristic(gattc_profile_t* ble_client)
{
    esp_err_t result = ESP_OK;

    return result;
}