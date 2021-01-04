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
#define FAIL_BIT                BIT15

static void esp_gap_callback(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
static void esp_gattc_callback(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);
static hub_ble_client* get_client(esp_gatt_if_t gattc_if);

static scan_callback_t scan_callback;

static const char* TAG = "HUB_BLE";

static esp_ble_scan_params_t ble_scan_params = {
    .scan_type = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval = 0x50,
    .scan_window = 0x30
};

static hub_ble_client* gl_profile_tab[PROFILE_NUM];
static int registered_apps;

static void esp_gap_callback(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    ESP_LOGI(TAG, "Function: %s", __func__);

    uint8_t *adv_name = NULL;
    uint8_t adv_name_len = 0;

    switch (event)
    {
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT:
        ESP_LOGI(TAG, "Set scan parameters complete.");
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
                    scan_callback(param->scan_rst.bda, (const char*)adv_name, param->scan_rst.ble_addr_type);
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
    default:
        ESP_LOGW(TAG, "Other GAP event: %i.", event);
        break;
    }
}

static void esp_gattc_callback(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param)
{
    ESP_LOGI(TAG, "Function: %s", __func__);

    esp_err_t result = ESP_OK;
    hub_ble_client* client = NULL;

    switch (event)
    {
    case ESP_GATTC_REG_EVT:
        ESP_LOGI(TAG, "ESP_GATTC_REG_EVT");

        client = gl_profile_tab[param->reg.app_id];

        if (param->reg.status == ESP_GATT_OK)
        {
            client->gattc_if = gattc_if;
            client->app_id = param->reg.app_id;

            ESP_LOGI(TAG, "Register app success, app_id %04x.", client->app_id);

            result = esp_ble_gap_set_scan_params(&ble_scan_params);
            if (result != ESP_OK)
            {
                ESP_LOGE(TAG, "Set scan parameters failed in function %s with error code %x.\n", __func__, result);
            }
        }
        else
        {
            ESP_LOGE(TAG, "Register app failed, app_id %04x, status %d", param->reg.app_id, param->reg.status);
            return;
        }

        break;
    case ESP_GATTC_CONNECT_EVT:
        ESP_LOGI(TAG, "ESP_GATTC_CONNECT_EVT");

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
        ESP_LOGI(TAG, "ESP_GATTC_OPEN_EVT");
        break;
    case ESP_GATTC_DIS_SRVC_CMPL_EVT:
        ESP_LOGI(TAG, "ESP_GATTC_DIS_SRVC_CMPL_EVT");
        break;
    case ESP_GATTC_CFG_MTU_EVT:
        ESP_LOGI(TAG, "ESP_GATTC_CFG_MTU_EVT");

        client = get_client(gattc_if);
        if (client == NULL)
        {
            ESP_LOGE(TAG, "Client not found.");
            break;
        }

        xEventGroupSetBits(client->event_group, CONNECT_BIT);
        break;
    case ESP_GATTC_SEARCH_RES_EVT:
        ESP_LOGI(TAG, "ESP_GATTC_SEARCH_RES_EVT");

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
        ESP_LOGI(TAG, "ESP_GATTC_SEARCH_CMPL_EVT");

        client = get_client(gattc_if);
        if (client == NULL)
        {
            ESP_LOGE(TAG, "Client not found.");
            break;
        }

        xEventGroupSetBits(client->event_group, SEARCH_SERVICE_BIT);
        break;
    case ESP_GATTC_REG_FOR_NOTIFY_EVT:
        ESP_LOGI(TAG, "ESP_GATTC_REG_FOR_NOTIFY_EVT");

        client = get_client(gattc_if);
        if (client == NULL)
        {
            ESP_LOGE(TAG, "Client not found.");
            break;
        }

        xEventGroupSetBits(client->event_group, REG_FOR_NOTIFY_BIT);
        break;
    case ESP_GATTC_NOTIFY_EVT:
        ESP_LOGI(TAG, "ESP_GATTC_NOTIFY_EVT");

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

        client->notify_cb(client, param);

        break;
    case ESP_GATTC_READ_CHAR_EVT:
        ESP_LOGI(TAG, "ESP_GATTC_READ_CHAR_EVT");

        client = get_client(gattc_if);
        if (client == NULL)
        {
            ESP_LOGE(TAG, "Client not found.");
            break;
        }

        xEventGroupSetBits(client->event_group, READ_CHAR_BIT);

        break;
    case ESP_GATTC_WRITE_DESCR_EVT:
        ESP_LOGI(TAG, "ESP_GATTC_WRITE_DESCR_EVT");

        client = get_client(gattc_if);
        if (client == NULL)
        {
            ESP_LOGE(TAG, "Client not found.");
            break;
        }

        xEventGroupSetBits(client->event_group, WRITE_DESCR_BIT);
        break;
    case ESP_GATTC_SRVC_CHG_EVT:
        ESP_LOGI(TAG, "ESP_GATTC_SRVC_CHG_EVT");
        break;
    case ESP_GATTC_WRITE_CHAR_EVT:
        ESP_LOGI(TAG, "ESP_GATTC_WRITE_CHAR_EVT");

        client = get_client(gattc_if);
        if (client == NULL)
        {
            ESP_LOGE(TAG, "Client not found.");
            break;
        }

        xEventGroupSetBits(client->event_group, WRITE_CHAR_BIT);
        break;
    case ESP_GATTC_DISCONNECT_EVT:
        ESP_LOGI(TAG, "ESP_GATTC_DISCONNECT_EVT");
        break;
    default:
        ESP_LOGW(TAG, "Other GATTC event: %i.", event);
        break;
    }
}

static hub_ble_client* get_client(esp_gatt_if_t gattc_if)
{
    for (int i = 0; i < PROFILE_NUM; i++)
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
    ESP_LOGI(TAG, "Function: %s", __func__);

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

    result = esp_ble_gap_register_callback(&esp_gap_callback);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "BLE GAP callback register failed in function %s with error code %x.\n", __func__, result);
        return result;
    }

    result = esp_ble_gattc_register_callback(&esp_gattc_callback);
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

    ESP_LOGI(TAG, "BLE initialized.");

    return result;
}

esp_err_t hub_ble_deinit()
{
    ESP_LOGI(TAG, "Function: %s", __func__);

    return ESP_OK;
}

esp_err_t hub_ble_start_scanning(uint16_t scan_time)
{
    return esp_ble_gap_start_scanning(scan_time);
}

esp_err_t hub_ble_register_scan_callback(scan_callback_t callback)
{
    ESP_LOGI(TAG, "Function: %s", __func__);

    if (callback == NULL)
    {
        return ESP_FAIL;
    }

    scan_callback = callback;
    return ESP_OK;
}

esp_err_t hub_ble_client_init(hub_ble_client* ble_client)
{
    ESP_LOGI(TAG, "Function: %s", __func__);
    esp_err_t result = ESP_OK;

    ble_client->gattc_if = ESP_GATT_IF_NONE;
    gl_profile_tab[registered_apps] = ble_client;
    result = esp_ble_gattc_app_register(registered_apps);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "BLE GATC app register failed in function %s with error code %x.\n", __func__, result);
        gl_profile_tab[registered_apps] = NULL;
        return result;
    }

    registered_apps++;
    ble_client->event_group = xEventGroupCreate();
    return result;
}

esp_err_t hub_ble_client_destroy(hub_ble_client* ble_client)
{
    ESP_LOGI(TAG, "Function: %s", __func__);

    vEventGroupDelete(ble_client->event_group);

    return ESP_OK;
}

esp_err_t hub_ble_client_connect(hub_ble_client* ble_client)
{
    ESP_LOGI(TAG, "Function: %s", __func__);
    esp_err_t result = ESP_OK;

    result = esp_ble_gap_stop_scanning();
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Scan stop failed, error: %i.", result);
        return result;
    }

    result = esp_ble_gattc_open(ble_client->gattc_if, ble_client->remote_bda, ble_client->addr_type, true);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "GATT connect failed, error: %i.", result);
        return result;
    }

    EventBits_t bits = xEventGroupWaitBits(ble_client->event_group, CONNECT_BIT | FAIL_BIT, pdTRUE, pdFALSE, BLE_TIMEOUT);

    if (!(bits & CONNECT_BIT))
    {
        ESP_LOGE(TAG, "Connection failed.");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "BLE client connection success.");
    return result;
}

esp_err_t hub_ble_client_register_for_notify(hub_ble_client* ble_client, uint16_t handle, notify_callback_t callback)
{
    ESP_LOGI(TAG, "Function: %s", __func__);
    esp_err_t result = ESP_OK;

    result = esp_ble_gattc_register_for_notify(ble_client->gattc_if, ble_client->remote_bda, handle);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Register for notify failed, error: %i.", result);
        return result;
    }

    EventBits_t bits = xEventGroupWaitBits(ble_client->event_group, REG_FOR_NOTIFY_BIT | FAIL_BIT, pdTRUE, pdFALSE, BLE_TIMEOUT);

    if (!(bits & REG_FOR_NOTIFY_BIT))
    {
        ESP_LOGE(TAG, "Register for notify failed.");
        return ESP_FAIL;
    }

    ble_client->notify_cb = callback;

    ESP_LOGI(TAG, "Register for notify success.");
    return result;
}

esp_err_t hub_ble_client_search_service(hub_ble_client* ble_client, esp_bt_uuid_t* uuid)
{
    ESP_LOGI(TAG, "Function: %s", __func__);
    esp_err_t result = ESP_OK;

    result = esp_ble_gattc_search_service(ble_client->gattc_if, ble_client->conn_id, uuid);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Search service failed, error: %i.", result);
        return result;
    }

    EventBits_t bits = xEventGroupWaitBits(ble_client->event_group, SEARCH_SERVICE_BIT | FAIL_BIT, pdTRUE, pdFALSE, BLE_TIMEOUT);

    if (!(bits & SEARCH_SERVICE_BIT))
    {
        ESP_LOGE(TAG, "Connection failed.");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Search service success.");
    return result;
}

esp_err_t hub_ble_client_write_characteristic(hub_ble_client* ble_client, uint16_t handle, uint8_t* value, uint16_t value_length)
{
    ESP_LOGI(TAG, "Function: %s", __func__);
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
        ESP_LOGE(TAG, "Write characteristic failed, error: %i.", result);
        return result;
    }

    EventBits_t bits = xEventGroupWaitBits(ble_client->event_group, WRITE_CHAR_BIT | FAIL_BIT, pdTRUE, pdFALSE, BLE_TIMEOUT);

    if (!(bits & WRITE_CHAR_BIT))
    {
        ESP_LOGE(TAG, "Write characteristic failed.");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Write characteristic success.");

    return result;
}

esp_err_t hub_ble_client_read_characteristic(hub_ble_client* ble_client, uint16_t handle)
{
    ESP_LOGI(TAG, "Function: %s", __func__);
    esp_err_t result = ESP_OK;

    result = esp_ble_gattc_read_char(
        ble_client->gattc_if, 
        ble_client->conn_id,
        handle,
        ESP_GATT_AUTH_REQ_NONE
    );

    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Read characteristic failed, error: %i.", result);
        return result;
    }

    EventBits_t bits = xEventGroupWaitBits(ble_client->event_group, READ_CHAR_BIT | FAIL_BIT, pdTRUE, pdFALSE, BLE_TIMEOUT);

    if (!(bits & READ_CHAR_BIT))
    {
        ESP_LOGE(TAG, "Read characteristic failed.");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Read characteristic success.");
    return result;
}

esp_gatt_status_t hub_ble_client_get_descriptors(hub_ble_client* ble_client, uint16_t char_handle, esp_gattc_descr_elem_t* descr, uint16_t* count)
{
    ESP_LOGI(TAG, "Function: %s", __func__);
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
        ESP_LOGE(TAG, "Get descriptors failed, error: %i.", result);
    }

    return result;
}

esp_err_t hub_ble_client_write_descriptor(hub_ble_client* ble_client, uint16_t handle, uint8_t* value, uint16_t value_length)
{
    ESP_LOGI(TAG, "Function: %s", __func__);
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
        ESP_LOGE(TAG, "Write descriptor failed, error: %i.", result);
        return result;
    }

    EventBits_t bits = xEventGroupWaitBits(ble_client->event_group, WRITE_DESCR_BIT | FAIL_BIT, pdTRUE, pdFALSE, BLE_TIMEOUT);

    if (!(bits & WRITE_DESCR_BIT))
    {
        ESP_LOGE(TAG, "Write descriptor failed.");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Write descriptor success.");
    return result;
}