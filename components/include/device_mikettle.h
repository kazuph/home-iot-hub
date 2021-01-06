#ifndef DEVICE_MIKETTLE_H
#define DEVICE_MIKETTLE_H

#include "stdint.h"
#include "esp_err.h"
#include "hub_ble.h"

#define MIKETTLE_DEVICE_NAME                "MiKettle"
#define MIKETTLE_MQTT_TOPIC                 "/mikettle"
#define MIKETTLE_JSON_FORMAT                "{\"temperature\":{\"current\":%i,\"set\":%i},\"action\":%i,\"mode\":%i,\"keep_warm\":{\"type\":%i,\"time\":%i,\"time_limit\":%i},\"boil_mode\":%i}"
#define MIKETTLE_PRODUCT_ID                 275

/* MiKettle services */
#define MIKETTLE_GATT_UUID_KETTLE_SRV       0xfe95
#define MIKETTLE_GATT_UUID_KETTLE_DATA_SRV  { 0x56, 0x61, 0x23, 0x37, 0x28, 0x26, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x36, 0x47, 0x34, 0x01 }

/* MiKettle characteristics */
#define MIKETTLE_GATT_UUID_AUTH_INIT        0x0010
#define MIKETTLE_GATT_UUID_AUTH             0x0001
#define MIKETTLE_GATT_UUID_VERSION          0x0004
#define MIKETTLE_GATT_UUID_SETUP            0xaa01
#define MIKETTLE_GATT_UUID_STATUS           0xaa02
#define MIKETTLE_GATT_UUID_TIME             0xaa04
#define MIKETTLE_GATT_UUID_BOIL_MODE        0xaa05
#define MIKETTLE_GATT_UUID_MCU_VERSION      0x2a28

/* MiKettle handles */
#define MIKETTLE_HANDLE_READ_FIRMWARE_VERSION   26
#define MIKETTLE_HANDLE_READ_NAME               20
#define MIKETTLE_HANDLE_AUTH_INIT               44
#define MIKETTLE_HANDLE_AUTH                    37
#define MIKETTLE_HANDLE_VERSION                 42
#define MIKETTLE_HANDLE_KEEP_WARM               58
#define MIKETTLE_HANDLE_STATUS                  61
#define MIKETTLE_HANDLE_TIME_LIMIT              65
#define MIKETTLE_HANDLE_BOIL_MODE               68

typedef union mikettle_status
{
    struct 
    {
        uint8_t action;
        uint8_t mode;
        uint16_t unknown;
        uint8_t temperature_set;
        uint8_t temperature_current;
        uint8_t keep_warm_type;
        uint16_t keep_warm_time;
    };
    uint8_t data[9];
} mikettle_status;

esp_err_t mikettle_authorize(hub_ble_client* ble_client, notify_callback_t callback);

#endif