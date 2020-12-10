#ifndef HUB_MQTT_H
#define HUB_MQTT_H

#include <stdint.h>

#include "esp_err.h"
#include "mqtt_client.h"

typedef struct hub_mqtt_client
{
    esp_mqtt_client_config_t config;
    esp_mqtt_client_handle_t _client_handle;
    esp_err_t (*start)(struct hub_mqtt_client* client);
    esp_err_t (*stop)(struct hub_mqtt_client* client);
    esp_err_t (*publish)(struct hub_mqtt_client* client, const char* topic, const char* data);
    esp_err_t (*subscribe)(struct hub_mqtt_client* client, const char* topic);
    esp_err_t (*unsubscribe)(struct hub_mqtt_client* client, const char* topic);
} hub_mqtt_client;

esp_err_t hub_mqtt_client_initialize(hub_mqtt_client* client);
esp_err_t hub_mqtt_client_destroy(hub_mqtt_client* client);

#endif