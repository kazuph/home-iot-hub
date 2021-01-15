#ifndef HUB_BLE_DEFINES_H
#define HUB_BLE_DEFINES_H

#include "stdint.h"

#include "esp_err.h"

#include "esp_bt_defs.h"
#include "esp_gatt_defs.h"
#include "esp_gattc_api.h"

typedef uint8_t hub_ble_client_handle_t;

typedef void (*scan_callback_t)(const char* device_name, esp_bd_addr_t address, esp_ble_addr_type_t address_type);
typedef void (*connect_callback_t)(const hub_ble_client_handle_t client_handle, esp_bd_addr_t address, esp_ble_addr_type_t address_type);
typedef void (*notify_callback_t)(const hub_ble_client_handle_t ble_client, uint16_t handle, uint8_t *value, uint16_t value_length);
typedef void (*disconnect_callback_t)(const hub_ble_client_handle_t ble_client);

/*
    Struct specifying BLE connect process and data
    exchange function pointers for the given device type.
    All the functions specified are called AFTER the default ones.
*/
typedef struct ble_device_type
{
    const char* name;                   // Device name
    connect_callback_t connect;         // BLE connect function
    notify_callback_t notify;           // BLE notify data function
    disconnect_callback_t disconnect;   // BLE disconnect function
} ble_device_type;

#endif