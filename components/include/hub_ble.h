#ifndef HUB_BLE_H
#define HUB_BLE_H

#include "esp_err.h"
#include "esp_bt_defs.h"

typedef void (*scan_callback_t)(esp_bd_addr_t address, const char* device_name);

esp_err_t hub_ble_init();

esp_err_t hub_ble_deinit();

esp_err_t hub_ble_register_scan_callback(scan_callback_t callback);

#endif