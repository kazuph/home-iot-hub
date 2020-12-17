#ifndef HUB_MQTT_H
#define HUB_MQTT_H

#include <stdint.h>

#include "esp_err.h"
#include "mqtt_client.h"

struct hub_mqtt_client;
typedef struct hub_mqtt_client hub_mqtt_client;

typedef esp_mqtt_client_config_t hub_mqtt_client_config;
typedef void (*hub_mqtt_client_data_callback_t)(hub_mqtt_client* client, const char* topic, const void* data, int length);

struct hub_mqtt_client
{
    esp_mqtt_client_handle_t _client_handle;
    hub_mqtt_client_data_callback_t _data_callback;
    esp_err_t (*start)(hub_mqtt_client* client);
    esp_err_t (*stop)(hub_mqtt_client* client);
    esp_err_t (*publish)(hub_mqtt_client* client, const char* topic, const char* data);
    esp_err_t (*subscribe)(hub_mqtt_client* client, const char* topic);
    esp_err_t (*unsubscribe)(hub_mqtt_client* client, const char* topic);
    esp_err_t (*register_data_callback)(hub_mqtt_client* client, hub_mqtt_client_data_callback_t callback);
};

/*
*   Initialize MQTT client with the specified config.
*/
esp_err_t hub_mqtt_client_initialize(hub_mqtt_client* client, const hub_mqtt_client_config* config);

/*
*   Destroy MQTT client.
*/
esp_err_t hub_mqtt_client_destroy(hub_mqtt_client* client);

#endif