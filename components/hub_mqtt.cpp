#include "hub_mqtt.h"

#include "esp_system.h"
#include "esp_event.h"

#include "esp_log.h"

namespace hub::mqtt
{
    constexpr const char *TAG = "HUB MQTT";

    void client::mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) 
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
        client* mqtt_client = reinterpret_cast<client*>(handler_args);
        
        switch (static_cast<int>(event->event_id)) 
        {
        case MQTT_EVENT_DATA:
            ESP_LOGV(TAG, "MQTT_EVENT_DATA");

            if (mqtt_client == nullptr)
            {
                ESP_LOGE(TAG, "Client not recognized.");
                break;
            }

            if (mqtt_client->data_callback == nullptr)
            {
                ESP_LOGE(TAG, "Client data callback not valid.");
                break;
            }
            
            mqtt_client->data_callback(std::string_view(event->topic, event->topic_len), std::string_view(event->data, event->data_len));           
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

    client::client(const client_config* const config)
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        data_callback = nullptr;
        client_handle = esp_mqtt_client_init(config);

        if (client_handle == nullptr)
        {
            ESP_LOGE(TAG, "MQTT client handle initialization failed.");
            return;
        }

        ESP_LOGI(TAG, "Client initialize success.");
    }

    client::~client()
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        
        esp_err_t result = ESP_OK;

        data_callback = nullptr;

        if (client_handle == nullptr)
        {
            return;
        }

        result = esp_mqtt_client_destroy(client_handle);
        if (result != ESP_OK)
        {
            ESP_LOGE(TAG, "Client destroy failed with error code %x [%s].", result, esp_err_to_name(result));
            return;
        }

        client_handle = nullptr;
        ESP_LOGI(TAG, "Client destroy success.");
    }

    esp_err_t client::start()
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        esp_err_t result = ESP_OK;

        if (client_handle == nullptr)
        {
            ESP_LOGE(TAG, "Client handle not initialized.");
            result = ESP_FAIL;
            return result;
        }

        result = esp_mqtt_client_register_event(
            client_handle, 
            static_cast<esp_mqtt_event_id_t>(MQTT_EVENT_DATA | MQTT_EVENT_ERROR | MQTT_EVENT_DISCONNECTED), 
            &mqtt_event_handler,
            this);
        if (result != ESP_OK)
        {
            ESP_LOGE(TAG, "Event registration failed with error code %x [%s].", result, esp_err_to_name(result));
            return result;
        }

        result = esp_mqtt_client_start(client_handle);
        if (result != ESP_OK)
        {
            ESP_LOGE(TAG, "Client start failed with error code %x [%s].", result, esp_err_to_name(result));
            return result;
        }

        ESP_LOGI(TAG, "Client start success.");
        return result;
    }

    esp_err_t client::stop()
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        return esp_mqtt_client_stop(client_handle);;
    }

    esp_err_t client::publish(std::string_view topic, std::string_view data, bool retain)
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        int message_id = esp_mqtt_client_publish(client_handle, topic.data(), data.data(), data.length(), 1, retain);
        
        if (message_id == -1)
        {
            ESP_LOGE(TAG, "Client publish failed.");
            return ESP_FAIL;
        }

        ESP_LOGD(TAG, "Published data:\n%s", data.data());
        ESP_LOGI(TAG, "Client publish success.");
        return ESP_OK;
    }

    esp_err_t client::subscribe(std::string_view topic)
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        int message_id = esp_mqtt_client_subscribe(client_handle, topic.data(), 0);

        if (message_id == -1)
        {
            ESP_LOGE(TAG, "Client subscribe failed.");
            return ESP_FAIL;
        }

        ESP_LOGI(TAG, "Client subscribe success");
        return ESP_OK;
    }

    esp_err_t client::unsubscribe(std::string_view topic)
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        int message_id = esp_mqtt_client_unsubscribe(client_handle, topic.data());

        if (message_id == -1)
        {
            ESP_LOGE(TAG, "Client unsubscribe failed.");
            return ESP_FAIL;
        }

        ESP_LOGI(TAG, "Client unsubscribe success.");
        return ESP_OK;
    }

    esp_err_t client::register_data_callback(data_callback_t callback)
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        data_callback = callback;

        ESP_LOGI(TAG, "Client callback set.");
        return ESP_OK;
    }
}