#ifndef HUB_BLE_H
#define HUB_BLE_H

#include "stdint.h"

#include "esp_err.h"
#include "esp_bt_defs.h"
#include "esp_gatt_defs.h"
#include "esp_gattc_api.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#define HUB_BLE_MAX_CLIENTS CONFIG_BTDM_CTRL_BLE_MAX_CONN

struct hub_ble_client;
typedef struct hub_ble_client hub_ble_client;

typedef void (*scan_callback_t)(esp_bd_addr_t address, const char* device_name, esp_ble_addr_type_t address_type);
typedef void (*notify_callback_t)(hub_ble_client* ble_client, uint16_t handle, uint8_t *value, uint16_t value_length);
typedef void (*disconnect_callback_t)(hub_ble_client* ble_client);

struct hub_ble_client
{
    uint16_t gattc_if;
    uint16_t app_id;
    uint16_t conn_id;
    uint16_t service_start_handle;
    uint16_t service_end_handle;
    esp_bd_addr_t remote_bda;
    esp_ble_addr_type_t addr_type;
    EventGroupHandle_t event_group;
    notify_callback_t notify_cb;
    disconnect_callback_t disconnect_cb;
    uint16_t* _buff_length;
    void* _buff;
};

esp_err_t hub_ble_init();

esp_err_t hub_ble_deinit();

esp_err_t hub_ble_start_scanning(uint32_t scan_time);

esp_err_t hub_ble_stop_scanning();

esp_err_t hub_ble_register_scan_callback(scan_callback_t callback);

esp_err_t hub_ble_client_init(hub_ble_client* ble_client);

esp_err_t hub_ble_client_destroy(hub_ble_client* ble_client);

esp_err_t hub_ble_client_connect(hub_ble_client* ble_client);

esp_err_t hub_ble_client_disconnect(hub_ble_client* ble_client);

esp_err_t hub_ble_client_register_for_notify(hub_ble_client* ble_client, uint16_t handle);

esp_err_t hub_ble_client_unregister_for_notify(hub_ble_client* ble_client, uint16_t handle);

esp_err_t hub_ble_client_register_notify_callback(hub_ble_client* ble_client, notify_callback_t callback);

esp_err_t hub_ble_client_register_disconnect_callback(hub_ble_client* ble_client, disconnect_callback_t callback);

esp_err_t hub_ble_client_get_services(hub_ble_client* ble_client, esp_gattc_service_elem_t* services, uint16_t* count);

esp_err_t hub_ble_client_get_service(hub_ble_client* ble_client, esp_bt_uuid_t* uuid);

esp_gatt_status_t hub_ble_client_get_characteristics(hub_ble_client* ble_client, esp_gattc_char_elem_t* characteristics, uint16_t* count);

esp_err_t hub_ble_client_write_characteristic(hub_ble_client* ble_client, uint16_t handle, uint8_t* value, uint16_t value_length);

esp_err_t hub_ble_client_read_characteristic(hub_ble_client* ble_client, uint16_t handle, uint8_t* value, uint16_t* value_length);

esp_gatt_status_t hub_ble_client_get_descriptors(hub_ble_client* ble_client, uint16_t char_handle, esp_gattc_descr_elem_t* descr, uint16_t* count);

esp_err_t hub_ble_client_write_descriptor(hub_ble_client* ble_client, uint16_t handle, uint8_t* value, uint16_t value_length);

esp_err_t hub_ble_client_read_descriptor(hub_ble_client* ble_client, uint16_t handle, uint8_t* value, uint16_t* value_length);

#endif