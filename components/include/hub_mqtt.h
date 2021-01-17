#ifndef HUB_MQTT_H
#define HUB_MQTT_H

#include "esp_err.h"
#include "stdbool.h"
#include "mqtt_client.h"

#ifdef __cplusplus
extern "C" {
#endif

struct hub_mqtt_client;
typedef struct hub_mqtt_client hub_mqtt_client;

typedef esp_mqtt_client_config_t hub_mqtt_client_config;
typedef void (*hub_mqtt_client_data_callback_t)(hub_mqtt_client* client, const char* topic, const void* data, int length);

struct hub_mqtt_client
{
    esp_mqtt_client_handle_t client_handle;
    hub_mqtt_client_data_callback_t data_callback;
};

esp_err_t hub_mqtt_client_init(hub_mqtt_client* client, const hub_mqtt_client_config* config);

esp_err_t hub_mqtt_client_destroy(hub_mqtt_client* client);

esp_err_t hub_mqtt_client_start(hub_mqtt_client* client);

esp_err_t hub_mqtt_client_stop(hub_mqtt_client* client);

esp_err_t hub_mqtt_client_publish(hub_mqtt_client* client, const char* topic, const char* data, bool retain);

esp_err_t hub_mqtt_client_subscribe(hub_mqtt_client* client, const char* topic);

esp_err_t hub_mqtt_client_unsubscribe(hub_mqtt_client* client, const char* topic);

esp_err_t hub_mqtt_client_register_subscribe_callback(hub_mqtt_client* client, hub_mqtt_client_data_callback_t callback);

esp_err_t hub_mqtt_client_init(hub_mqtt_client* client, const hub_mqtt_client_config* config);

esp_err_t hub_mqtt_client_destroy(hub_mqtt_client* client);

#ifdef __cplusplus
}
#endif

#endif