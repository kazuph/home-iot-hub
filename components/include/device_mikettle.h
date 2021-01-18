#ifndef DEVICE_MIKETTLE_H
#define DEVICE_MIKETTLE_H

#include "stdint.h"
#include "esp_err.h"
#include "hub_ble.h"

#define XIAOMI_MI_KETTLE_DEVICE_NAME "MiKettle"

typedef void (*data_ready_callback_t)(const hub_ble_client_handle_t client_handle, const char* data, uint16_t data_length);

esp_err_t mikettle_connect(const hub_ble_client_handle_t client_handle, esp_bd_addr_t address, esp_ble_addr_type_t address_type);

esp_err_t mikettle_reconnect(const hub_ble_client_handle_t client_handle);

esp_err_t mikettle_register_data_ready_callback(const hub_ble_client_handle_t client_handle, data_ready_callback_t notify_callback);

esp_err_t mikettle_update_data(const hub_ble_client_handle_t client_handle, const char* data, uint16_t data_length);

#endif