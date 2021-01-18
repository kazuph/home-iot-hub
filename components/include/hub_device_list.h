#ifndef HUB_DEVICE_LIST_H
#define HUB_DEVICE_LIST_H

#include "hub_ble.h"
#include "stdbool.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef esp_err_t (*connect_fun_t)(const hub_ble_client_handle_t client_handle, esp_bd_addr_t address, esp_ble_addr_type_t address_type);
typedef esp_err_t (*reconnect_fun_t)(const hub_ble_client_handle_t client_handle);
typedef void (*data_ready_callback_t)(const hub_ble_client_handle_t client_handle, const char* data, uint16_t data_length);
typedef esp_err_t (*register_data_ready_callback_fun_t)(const hub_ble_client_handle_t client_handle, data_ready_callback_t data_ready_callback);
typedef esp_err_t (*update_data_fun_t)(const hub_ble_client_handle_t client_handle, const char* data, uint16_t data_length);

/*
    Struct specifying BLE connect process and data
    exchange function pointers for the given device type.

    name - name of the device
    connect - function allowing custom connection scheme for the given device
    reconnect - reconnect function, handle is initialized by now, address and address type are memorized
    register_data_ready_callback - register function that gets called whenever data is updated via BLE AND formatted as JSON string.
    update_data - update_data device status request, the implementation is supposed to read JSON formated string and pass the data to BLE device.
*/
typedef struct ble_device_type
{
    const char* name;
    connect_fun_t connect;
    reconnect_fun_t reconnect;
    register_data_ready_callback_fun_t register_data_ready_callback;
    update_data_fun_t update_data;
} ble_device_type;

/*
    Get pointer to the ble_device_type instance for the given device name.
*/
const ble_device_type* get_type_for_device_name(const char* name);

/*
    Check if device is suported.
*/
bool is_device_supported(const char* name);

#ifdef __cplusplus
}
#endif

#endif