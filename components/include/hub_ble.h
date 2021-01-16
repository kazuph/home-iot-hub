#ifndef HUB_BLE_H
#define HUB_BLE_H

#include "hub_ble_defines.h"

#include "stdint.h"

esp_err_t hub_ble_init();

esp_err_t hub_ble_deinit();

esp_err_t hub_ble_start_scanning(uint32_t scan_time);

esp_err_t hub_ble_stop_scanning();

esp_err_t hub_ble_register_scan_callback(scan_callback_t callback);

esp_err_t hub_ble_client_init(hub_ble_client_handle_t* client_handle);

esp_err_t hub_ble_client_destroy(const hub_ble_client_handle_t ble_client);

esp_err_t hub_ble_client_connect(const hub_ble_client_handle_t ble_client, esp_bd_addr_t address, esp_ble_addr_type_t address_type);

esp_err_t hub_ble_client_reconnect(const hub_ble_client_handle_t ble_client);

esp_err_t hub_ble_client_disconnect(const hub_ble_client_handle_t ble_client);

esp_err_t hub_ble_client_register_for_notify(const hub_ble_client_handle_t ble_client, uint16_t handle);

esp_err_t hub_ble_client_unregister_for_notify(const hub_ble_client_handle_t ble_client, uint16_t handle);

esp_err_t hub_ble_client_register_notify_callback(const hub_ble_client_handle_t ble_client, notify_callback_t callback);

esp_err_t hub_ble_client_register_disconnect_callback(const hub_ble_client_handle_t ble_client, disconnect_callback_t callback);

esp_err_t hub_ble_client_get_services(const hub_ble_client_handle_t ble_client, esp_gattc_service_elem_t* services, uint16_t* count);

esp_err_t hub_ble_client_get_service(const hub_ble_client_handle_t ble_client, esp_bt_uuid_t* uuid);

esp_gatt_status_t hub_ble_client_get_characteristics(const hub_ble_client_handle_t ble_client, esp_gattc_char_elem_t* characteristics, uint16_t* count);

esp_err_t hub_ble_client_write_characteristic(const hub_ble_client_handle_t ble_client, uint16_t handle, uint8_t* value, uint16_t value_length);

esp_err_t hub_ble_client_read_characteristic(const hub_ble_client_handle_t ble_client, uint16_t handle, uint8_t* value, uint16_t* value_length);

esp_gatt_status_t hub_ble_client_get_descriptors(const hub_ble_client_handle_t ble_client, uint16_t char_handle, esp_gattc_descr_elem_t* descr, uint16_t* count);

esp_err_t hub_ble_client_write_descriptor(const hub_ble_client_handle_t ble_client, uint16_t handle, uint8_t* value, uint16_t value_length);

esp_err_t hub_ble_client_read_descriptor(const hub_ble_client_handle_t ble_client, uint16_t handle, uint8_t* value, uint16_t* value_length);

#endif