#include "hub_mqtt.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"

#include "lwip/err.h"
#include "lwip/sys.h"

static const char *TAG = "HUB MQTT";

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) 
{
    ESP_LOGD(TAG, "Function: %s.", __func__);

    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    
    switch (event->event_id) {
        case MQTT_EVENT_DATA:
            ESP_LOGV(TAG, "MQTT_EVENT_DATA");
            hub_mqtt_client* client = (hub_mqtt_client*)handler_args;

            if (client == NULL)
            {
                ESP_LOGE(TAG, "Client not recognized.");
                break;
            }

            if (client->data_callback == NULL)
            {
                ESP_LOGE(TAG, "Client data callback not valid.");
                break;
            }

            client->data_callback(client, event->topic, event->data, event->data_len);           
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGW(TAG, "MQTT_EVENT_ERROR");
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "MQTT_EVENT_DISCONNECTED");
            break;
        default:
            ESP_LOGW(TAG, "Unknown event. ID: %d.", event->event_id);
            break;
    }
}

esp_err_t hub_mqtt_client_start(hub_mqtt_client* client)
{
    ESP_LOGD(TAG, "Function: %s.", __func__);

    esp_err_t result = ESP_OK;

    if (client->client_handle == NULL)
    {
        ESP_LOGE(TAG, "Client handle not initialized.");
        result = ESP_FAIL;
        return result;
    }

    result = esp_mqtt_client_register_event(client->client_handle, MQTT_EVENT_DATA | MQTT_EVENT_ERROR | MQTT_EVENT_DISCONNECTED, mqtt_event_handler, client);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Event registration failed with error code %x [%s].", result, esp_err_to_name(result));
        return result;
    }

    result = esp_mqtt_client_start(client->client_handle);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Client start failed with error code %x [%s].", result, esp_err_to_name(result));
        return result;
    }

    ESP_LOGI(TAG, "Client start success.");
    return result;
}

esp_err_t hub_mqtt_client_stop(hub_mqtt_client* client)
{
    ESP_LOGD(TAG, "Function: %s.", __func__);
    return esp_mqtt_client_stop(client->client_handle);;
}

esp_err_t hub_mqtt_client_publish(hub_mqtt_client* client, const char* topic, const char* data, bool retain)
{
    ESP_LOGD(TAG, "Function: %s.", __func__);

    int message_id = esp_mqtt_client_publish(client->client_handle, topic, data, strlen(data), 1, retain);
    
    if (message_id == -1)
    {
        ESP_LOGE(TAG, "Client publish failed.");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Client publish success.");
    return ESP_OK;
}

esp_err_t hub_mqtt_client_subscribe(hub_mqtt_client* client, const char* topic)
{
    ESP_LOGD(TAG, "Function: %s.", __func__);

    int message_id = esp_mqtt_client_subscribe(client->client_handle, topic, 0);

    if (message_id == -1)
    {
        ESP_LOGE(TAG, "Client subscribe failed.");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Client subscribe success");
    return ESP_OK;
}

esp_err_t hub_mqtt_client_unsubscribe(hub_mqtt_client* client, const char* topic)
{
    ESP_LOGD(TAG, "Function: %s.", __func__);

    int message_id = esp_mqtt_client_unsubscribe(client->client_handle, topic);

    if (message_id == -1)
    {
        ESP_LOGE(TAG, "Client unsubscribe failed.");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Client unsubscribe success.");
    return ESP_OK;
}

esp_err_t hub_mqtt_client_register_subscribe_callback(hub_mqtt_client* client, hub_mqtt_client_data_callback_t callback)
{
    ESP_LOGD(TAG, "Function: %s.", __func__);

    client->data_callback = callback;

    ESP_LOGI(TAG, "Client callback set.");
    return ESP_OK;
}

esp_err_t hub_mqtt_client_init(hub_mqtt_client* client, const hub_mqtt_client_config* config)
{
    ESP_LOGD(TAG, "Function: %s.", __func__);

    client->data_callback = NULL;
    client->client_handle = esp_mqtt_client_init(config);

    if (client->client_handle == NULL)
    {
        ESP_LOGE(TAG, "MQTT client handle initialization failed.");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Client initialize success.");
    return ESP_OK;
}

esp_err_t hub_mqtt_client_destroy(hub_mqtt_client* client)
{
    ESP_LOGD(TAG, "Function: %s.", __func__);
    
    esp_err_t result = ESP_OK;

    client->data_callback = NULL;

    result = esp_mqtt_client_destroy(client->client_handle);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Client destroy failed with error code %x [%s].", result, esp_err_to_name(result));
        return result;
    }

    client->client_handle = NULL;

    ESP_LOGI(TAG, "Client destroy success.");
    return result;
}