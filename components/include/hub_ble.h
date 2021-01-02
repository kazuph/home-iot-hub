#ifndef HUB_BLE_H
#define HUB_BLE_H

#include "esp_err.h"
#include "esp_bt_defs.h"
#include "hub_ble_global.h"

typedef void (*scan_callback_t)(esp_bd_addr_t address, const char* device_name);

esp_err_t hub_ble_init();

esp_err_t hub_ble_deinit();

esp_err_t hub_ble_register_scan_callback(scan_callback_t callback);

esp_err_t hub_ble_client_init(gattc_profile_t* ble_client);

esp_err_t hub_ble_client_destroy(gattc_profile_t* ble_client);

esp_err_t hub_ble_client_connect(gattc_profile_t* ble_client);

esp_err_t hub_ble_client_search_service(gattc_profile_t* ble_client);

esp_err_t hub_ble_client_write_characteristic(gattc_profile_t* ble_client, uint16_t handle, uint8_t* value, uint16_t value_length);

esp_err_t hub_ble_client_read_characteristic(gattc_profile_t* ble_client);

#endif