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

#define PROFILE_NUM 1

struct hub_ble_client;
typedef struct hub_ble_client hub_ble_client;

typedef void (*scan_callback_t)(esp_bd_addr_t address, const char* device_name, esp_ble_addr_type_t address_type);
typedef void (*notify_callback_t)(hub_ble_client* ble_client, struct gattc_notify_evt_param* param);

struct hub_ble_client
{
    uint16_t gattc_if;
    uint16_t app_id;
    uint16_t conn_id;
    uint16_t service_start_handle;
    uint16_t service_end_handle;
    uint16_t char_handle;
    esp_ble_addr_type_t addr_type;
    esp_bd_addr_t remote_bda;
    EventGroupHandle_t event_group;
    notify_callback_t notify_cb;
};

esp_err_t hub_ble_init();

esp_err_t hub_ble_deinit();

esp_err_t hub_ble_start_scanning(uint32_t scan_time);

esp_err_t hub_ble_register_scan_callback(scan_callback_t callback);

esp_err_t hub_ble_client_init(hub_ble_client* ble_client);

esp_err_t hub_ble_client_destroy(hub_ble_client* ble_client);

esp_err_t hub_ble_client_connect(hub_ble_client* ble_client);

esp_err_t hub_ble_client_register_for_notify(hub_ble_client* ble_client, uint16_t handle, notify_callback_t callback);

esp_err_t hub_ble_client_unregister_for_notify(hub_ble_client* ble_client, uint16_t handle);

esp_err_t hub_ble_client_search_service(hub_ble_client* ble_client, esp_bt_uuid_t* uuid);

esp_err_t hub_ble_client_write_characteristic(hub_ble_client* ble_client, uint16_t handle, uint8_t* value, uint16_t value_length);

esp_err_t hub_ble_client_read_characteristic(hub_ble_client* ble_client, uint16_t handle);

esp_gatt_status_t hub_ble_client_get_descriptors(hub_ble_client* ble_client, uint16_t char_handle, esp_gattc_descr_elem_t* descr, uint16_t* count);

esp_err_t hub_ble_client_write_descriptor(hub_ble_client* ble_client, uint16_t handle, uint8_t* value, uint16_t value_length);

#endif