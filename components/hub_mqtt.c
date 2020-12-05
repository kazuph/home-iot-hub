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
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            break;
        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, message_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, message_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, message_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
            printf("DATA=%.*s\r\n", event->data_len, event->data);
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            break;
        default:
            ESP_LOGI(TAG, "Other event id:%d", event->event_id);
            break;
    }
}

static esp_err_t _hub_mqtt_client_initialize(hub_mqtt_client* client)
{
    esp_err_t result = ESP_OK;

    client->_client_handle = esp_mqtt_client_init(&(client->_mqtt_config));

    if (client->_client_handle == NULL)
    {
        ESP_LOGE(TAG, "MQTT client handle initialization failed.");
        result = ESP_FAIL;
        goto cleanup_mqtt_client_init;
    }

    result = esp_mqtt_client_register_event(client->_client_handle, ESP_EVENT_ANY_ID, mqtt_event_handler, client);

    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "MQTT event registration failed.");
        goto cleanup_mqtt_client_init;
    }

    result = esp_mqtt_client_start(client->_client_handle);

    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "MQTT client start failed.");
        goto cleanup_mqtt_client_init;
    }

    return result;

cleanup_mqtt_client_init:
    esp_mqtt_client_destroy(client->_client_handle);

    return result;
}

static esp_err_t _hub_mqtt_client_destroy(hub_mqtt_client* client)
{
    esp_mqtt_client_stop(client->_client_handle);
    esp_mqtt_client_destroy(client->_client_handle);

    return ESP_OK;
}

static esp_err_t _hub_mqtt_client_publish(hub_mqtt_client* client, const char* topic, const char* data)
{
    int message_id = esp_mqtt_client_publish(client->_client_handle, topic, data, strlen(data), 1, 0);
    
    if (message_id == -1)
    {
        ESP_LOGE(TAG, "Client publish failed.");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Client publish successful, message_id=%d", message_id);
    return ESP_OK;
}

static esp_err_t _hub_mqtt_client_subscribe(hub_mqtt_client* client, const char* topic)
{
    int message_id = esp_mqtt_client_subscribe(client->_client_handle, topic, 0);

    if (message_id == -1)
    {
        ESP_LOGE(TAG, "Client subscribe failed.");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Client subscribe successful, message_id=%d", message_id);
    return ESP_OK;
}

static esp_err_t _hub_mqtt_client_unsubscribe(hub_mqtt_client* client, const char* topic)
{
    int message_id = esp_mqtt_client_unsubscribe(client->_client_handle, topic);

    if (message_id == -1)
    {
        ESP_LOGE(TAG, "Client unsubscribe failed.");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Client unsubscribe successful, message_id=%d", message_id);
    return ESP_OK;
}

hub_mqtt_client create_hub_mqtt_client()
{
    hub_mqtt_client client = {
        ._mqtt_config = {
            .uri = CONFIG_MQTT_URI,
            .port = CONFIG_MQTT_PORT
        },
        .initialize = &_hub_mqtt_client_initialize,
        .destroy = &_hub_mqtt_client_destroy,
        .publish = &_hub_mqtt_client_publish,
        .subscribe = &_hub_mqtt_client_subscribe,
        .unsubscribe = &_hub_mqtt_client_unsubscribe,
    };
    return client;
}