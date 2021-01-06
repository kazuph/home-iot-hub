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
            ESP_LOGV(TAG, "MQTT_EVENT_DATA\n");
            hub_mqtt_client* client = (hub_mqtt_client*)handler_args;

            if (client == NULL)
            {
                ESP_LOGE(TAG, "MQTT client not recognized.");
                break;
            }

            if (client->_data_callback == NULL)
            {
                ESP_LOGE(TAG, "MQTT client data callback not valid.");
                break;
            }

            client->_data_callback(client, event->topic, event->data, event->data_len);           
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT_EVENT_ERROR\n");
            break;
        default:
            ESP_LOGW(TAG, "Unknown event. ID: %d\n", event->event_id);
            break;
    }
}

esp_err_t hub_mqtt_client_start(hub_mqtt_client* client)
{
    ESP_LOGD(TAG, "Function: %s.", __func__);

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

esp_err_t hub_mqtt_client_stop(hub_mqtt_client* client)
{
    ESP_LOGD(TAG, "Function: %s.", __func__);
    return esp_mqtt_client_stop(client->_client_handle);;
}

esp_err_t hub_mqtt_client_publish(hub_mqtt_client* client, const char* topic, const char* data)
{
    ESP_LOGD(TAG, "Function: %s.", __func__);

    int message_id = esp_mqtt_client_publish(client->_client_handle, topic, data, strlen(data), 1, 0);
    
    if (message_id == -1)
    {
        ESP_LOGE(TAG, "Client publish failed.\n");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Client publish successful, message_id = %d\n", message_id);
    return ESP_OK;
}

esp_err_t hub_mqtt_client_subscribe(hub_mqtt_client* client, const char* topic)
{
    ESP_LOGD(TAG, "Function: %s.", __func__);

    int message_id = esp_mqtt_client_subscribe(client->_client_handle, topic, 0);

    if (message_id == -1)
    {
        ESP_LOGE(TAG, "Client subscribe failed.\n");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Client subscribe successful, message_id = %d\n", message_id);
    return ESP_OK;
}

esp_err_t hub_mqtt_client_unsubscribe(hub_mqtt_client* client, const char* topic)
{
    ESP_LOGD(TAG, "Function: %s.", __func__);

    int message_id = esp_mqtt_client_unsubscribe(client->_client_handle, topic);

    if (message_id == -1)
    {
        ESP_LOGE(TAG, "Client unsubscribe failed.\n");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Client unsubscribe successful, message_id = %d\n", message_id);
    return ESP_OK;
}

esp_err_t hub_mqtt_client_register_subscribe_callback(hub_mqtt_client* client, hub_mqtt_client_data_callback_t callback)
{
    ESP_LOGD(TAG, "Function: %s.", __func__);

    client->_data_callback = callback;
    return ESP_OK;
}

esp_err_t hub_mqtt_client_init(hub_mqtt_client* client, const hub_mqtt_client_config* config)
{
    ESP_LOGD(TAG, "Function: %s.", __func__);

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
    ESP_LOGD(TAG, "Function: %s.", __func__);
    
    esp_err_t result = ESP_OK;

    client->_data_callback = NULL;
    result = esp_mqtt_client_destroy(client->_client_handle);
    client->_client_handle = NULL;

    return result;
}