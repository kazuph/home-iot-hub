#ifndef HUB_BLE_GLOBAL_H
#define HUB_BLE_GLOBAL_H

#include "stdint.h"
#include "esp_bt_defs.h"
#include "esp_gattc_api.h"
#include "esp_gap_ble_api.h"

#define PROFILE_NUM 1

typedef struct gattc_profile_t
{
    uint16_t gattc_if;
    uint16_t app_id;
    uint16_t conn_id;
    uint16_t service_start_handle;
    uint16_t service_end_handle;
    uint16_t char_handle;
    esp_bd_addr_t remote_bda;
    esp_ble_gattc_cb_param_t _data;
    esp_gattc_cb_event_t _last_event;
} gattc_profile_t;

extern gattc_profile_t gl_profile_tab[PROFILE_NUM];
extern esp_ble_scan_params_t ble_scan_params;

#endif