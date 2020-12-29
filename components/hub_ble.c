#include "hub_ble.h"

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_gatt_defs.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

#define PROFILE_NUM 1
#define PROFILE_A_APP_ID 0

typedef struct gattc_profile_t
{
    esp_gattc_cb_t gattc_cb;
    uint16_t gattc_if;
    uint16_t app_id;
    uint16_t conn_id;
    uint16_t service_start_handle;
    uint16_t service_end_handle;
    uint16_t char_handle;
    esp_bd_addr_t remote_bda;
} gattc_profile_t;

static void esp_gap_callback(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
static void esp_gattc_callback(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);
static void gattc_profile_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);

static const char* TAG = "HUB_BLE";

static esp_ble_scan_params_t ble_scan_params = {
    .scan_type = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval = 0x50,
    .scan_window = 0x30
};

static gattc_profile_t gl_profile_tab[PROFILE_NUM] = {
    [PROFILE_A_APP_ID] = {
        .gattc_cb = &gattc_profile_event_handler,
        .gattc_if = ESP_GATT_IF_NONE, /* Not get the gatt_if, so initial is ESP_GATT_IF_NONE */
    },
};

static void esp_gap_callback(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{

}

static void esp_gattc_callback(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param)
{
    /* If event is register event, store the gattc_if for each profile */
    if (event == ESP_GATTC_REG_EVT)
    {
        if (param->reg.status == ESP_GATT_OK)
        {
            gl_profile_tab[param->reg.app_id].gattc_if = gattc_if;
        }
        else
        {
            ESP_LOGI(TAG, "reg app failed, app_id %04x, status %d", param->reg.app_id, param->reg.status);
            return;
        }
    }

    for (int i = 0; i < PROFILE_NUM; i++)
    {
        if (gattc_if == ESP_GATT_IF_NONE || gattc_if == gl_profile_tab[i].gattc_if)
        {
            if (gl_profile_tab[i].gattc_cb == NULL)
            {
                continue;
            }

            gl_profile_tab[i].gattc_cb(event, gattc_if, param);
        }
    }
}

static void gattc_profile_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param)
{
    esp_err_t result = ESP_OK;

    switch (event)
    {
    case ESP_GATTC_REG_EVT:
        ESP_LOGI(TAG, "REG_EVT");
        result = esp_ble_gap_set_scan_params(&ble_scan_params);
        if (result != ESP_OK)
        {
            ESP_LOGE(TAG, "set scan params error, error code = %x", result);
        }
        break;
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT:
        esp_ble_gap_start_scanning(30U /* seconds */);
        break;
    case ESP_GAP_BLE_SCAN_RESULT_EVT:

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

    result = esp_ble_gattc_register_callback(esp_gattc_callback);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "BLE GATC callback register failed in function %s with error code %x.\n", __func__, result);
        return result;
    }

    result = esp_ble_gattc_app_register(PROFILE_A_APP_ID);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "BLE GATC app register failed in function %s with error code %x.\n", __func__, result);
        return result;
    }

    result = esp_ble_gatt_set_local_mtu(500);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "BLE GATT set local MTU failed in function %s with error code %x.\n", __func__, result);
        return result;
    }

    return result;
}

esp_err_t hub_ble_deinit()
{
    return ESP_OK;
}