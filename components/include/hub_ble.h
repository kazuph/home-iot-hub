#ifndef HUB_BLE_H
#define HUB_BLE_H

#include "stdint.h"
#include "stdbool.h"
#include "esp_bt_defs.h"
#include "esp_gattc_api.h"
#include "esp_gap_ble_api.h"
#include "esp_err.h"

#define PROFILE_NUM 1

typedef struct hub_ble_client
{
    uint16_t gattc_if;
    uint16_t app_id;
    uint16_t conn_id;
    uint16_t service_start_handle;
    uint16_t service_end_handle;
    uint16_t char_handle;
    esp_ble_addr_type_t addr_type;
    esp_bd_addr_t remote_bda;
} hub_ble_client;

typedef void (*scan_callback_t)(esp_bd_addr_t address, const char* device_name, esp_ble_addr_type_t address_type);

extern hub_ble_client* gl_profile_tab[PROFILE_NUM];
extern esp_ble_scan_params_t ble_scan_params;

esp_err_t hub_ble_init();

esp_err_t hub_ble_deinit();

esp_err_t hub_ble_register_scan_callback(scan_callback_t callback);

esp_err_t hub_ble_client_init(hub_ble_client* ble_client);

esp_err_t hub_ble_client_destroy(hub_ble_client* ble_client);

esp_err_t hub_ble_client_connect(hub_ble_client* ble_client);

esp_err_t hub_ble_client_search_service(hub_ble_client* ble_client, esp_bt_uuid_t* uuid);

esp_err_t hub_ble_client_write_characteristic(hub_ble_client* ble_client, uint16_t handle, uint8_t* value, uint16_t value_length);

esp_err_t hub_ble_client_read_characteristic(hub_ble_client* ble_client);

#endif