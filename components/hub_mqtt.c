#include "hub_mqtt.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"

#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

static const char *TAG = "HUB MQTT";

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) 
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

    // your_context_t *context = event->context;
    switch (event->event_id) {
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA\n");
            hub_mqtt_client* client = (hub_mqtt_client*)handler_args;
            client->_data_callback(client, event->topic, event->data, event->data_len);           
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR\n");
            break;
        default:
            ESP_LOGI(TAG, "Unknown event. ID: %d\n", event->event_id);
            break;
    }
}

static esp_err_t _hub_mqtt_client_start(hub_mqtt_client* client)
{
    esp_err_t result = ESP_OK;

    if (client->_client_handle == NULL)
    {
        ESP_LOGE(TAG, "MQTT client handle not initialized.\n");
        result = ESP_FAIL;
        return result;
    }

    result = esp_mqtt_client_register_event(client->_client_handle, MQTT_EVENT_DATA | MQTT_EVENT_ERROR, mqtt_event_handler, client);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "MQTT event registration failed.\n");
        return result;
    }

    result = esp_mqtt_client_start(client->_client_handle);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "MQTT client start failed.\n");
        return result;
    }

    return result;
}

static esp_err_t _hub_mqtt_client_stop(hub_mqtt_client* client)
{
    return esp_mqtt_client_stop(client->_client_handle);;
}

static esp_err_t _hub_mqtt_client_publish(hub_mqtt_client* client, const char* topic, const char* data)
{
    int message_id = esp_mqtt_client_publish(client->_client_handle, topic, data, strlen(data), 1, 0);
    
    if (message_id == -1)
    {
        ESP_LOGE(TAG, "Client publish failed.\n");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Client publish successful, message_id = %d\n", message_id);
    return ESP_OK;
}

static esp_err_t _hub_mqtt_client_subscribe(hub_mqtt_client* client, const char* topic)
{
    int message_id = esp_mqtt_client_subscribe(client->_client_handle, topic, 0);

    if (message_id == -1)
    {
        ESP_LOGE(TAG, "Client subscribe failed.\n");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Client subscribe successful, message_id = %d\n", message_id);
    return ESP_OK;
}

static esp_err_t _hub_mqtt_client_unsubscribe(hub_mqtt_client* client, const char* topic)
{
    int message_id = esp_mqtt_client_unsubscribe(client->_client_handle, topic);

    if (message_id == -1)
    {
        ESP_LOGE(TAG, "Client unsubscribe failed.\n");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Client unsubscribe successful, message_id = %d\n", message_id);
    return ESP_OK;
}

static esp_err_t _hub_mqtt_client_register_subscribe_callback(hub_mqtt_client* client, hub_mqtt_client_data_callback_t callback)
{
    client->_data_callback = callback;
    return ESP_OK;
}

esp_err_t hub_mqtt_client_initialize(hub_mqtt_client* client, const hub_mqtt_client_config* config)
{
    client->start = &_hub_mqtt_client_start;
    client->stop = &_hub_mqtt_client_stop;
    client->publish = &_hub_mqtt_client_publish;
    client->subscribe = &_hub_mqtt_client_subscribe;
    client->unsubscribe = &_hub_mqtt_client_unsubscribe;
    client->register_data_callback = &_hub_mqtt_client_register_subscribe_callback;
    client->_data_callback = NULL;

    client->_client_handle = esp_mqtt_client_init(config);
    if (client->_client_handle == NULL)
    {
        ESP_LOGE(TAG, "MQTT client handle initialization failed.\n");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t hub_mqtt_client_destroy(hub_mqtt_client* client)
{
    esp_err_t result = ESP_OK;

    client->start = NULL;
    client->stop = NULL;
    client->publish = NULL;
    client->subscribe = NULL;
    client->unsubscribe = NULL;
    client->register_data_callback = NULL;
    client->_data_callback = NULL;

    result = esp_mqtt_client_destroy(client->_client_handle);
    client->_client_handle = NULL;

    return result;
}