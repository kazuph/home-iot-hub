#include "mqtt.hpp"

#include "esp_system.h"
#include "esp_event.h"

#include "esp_log.h"

#include <exception>
#include <stdexcept>
#include <cassert>

namespace hub::mqtt
{
    void client::mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) 
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        esp_mqtt_event_handle_t event   = reinterpret_cast<esp_mqtt_event_handle_t>(event_data);
        client* mqtt_client             = reinterpret_cast<client*>(handler_args);

        if (event->event_id == MQTT_EVENT_DATA)
        {
            if (mqtt_client == nullptr)
            {
                ESP_LOGE(TAG, "Client not recognized.");
                return;
            }

            if (mqtt_client->m_data_event_handler)
            {
                mqtt_client->m_data_event_handler.invoke({ std::string(event->topic, event->topic_len), std::string(event->data, event->data_len) });   
            }
        }
        else if (event->event_id == MQTT_EVENT_CONNECTED)
        {

        }
        else if (event->event_id == MQTT_EVENT_DISCONNECTED)
        {

        }
        else if (event->event_id == MQTT_EVENT_ERROR)
        {

        }
    }

    client::client() :
        m_client_handle       { nullptr },
        m_data_event_handler  {  }
    {
        ESP_LOGD(TAG, "Function: %s. (default constructor)", __func__);
    }

    client::client(client&& other) :
        m_client_handle       { nullptr },
        m_data_event_handler  {  }
    {
        ESP_LOGD(TAG, "Function: %s. (move constructor)", __func__);

        *this = std::move(other);
    }

    client::~client()
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        if (esp_err_t result = disconnect(); result != ESP_OK)
        {
            ESP_LOGW(TAG, "Client disconnect failed with error code %x [%s].", result, esp_err_to_name(result));
        }
    }

    client& client::operator=(client&& other)
    {
        ESP_LOGD(TAG, "Function: %s. (move assignment)", __func__);

        if (this == &other)
        {
            return *this;
        }

        m_client_handle       = other.m_client_handle;
        other.m_client_handle = nullptr;

        return *this;
    }

    esp_err_t client::connect(std::string_view uri)
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        esp_err_t result = ESP_OK;

        {
            esp_mqtt_client_config_t mqtt_client_config{};

            mqtt_client_config.uri  = uri.data();
            m_client_handle         = esp_mqtt_client_init(&mqtt_client_config);
        }

        if (!m_client_handle)
        {
            ESP_LOGE(TAG, "Client handle initialization failed.");
            return ESP_FAIL;
        }

        if (result = esp_mqtt_client_start(m_client_handle); result != ESP_OK)
        {
            ESP_LOGE(TAG, "Client start failed with error code %x [%s].", result, esp_err_to_name(result));
            return result;
        }

        ESP_LOGI(TAG, "Client start success.");

        return result;
    }

    esp_err_t client::disconnect()
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        esp_err_t result = ESP_OK;

        if (m_client_handle == nullptr)
        {
            ESP_LOGW(TAG, "Client handle was nullptr.");
            return ESP_FAIL;
        }

        esp_mqtt_client_stop(m_client_handle);
        esp_mqtt_client_destroy(m_client_handle);

        m_client_handle = nullptr;
        return result;
    }

    esp_err_t client::publish(std::string_view topic, std::string_view data, bool retain)
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        assert(m_client_handle);
        
        if (esp_mqtt_client_publish(m_client_handle, topic.data(), data.data(), data.length(), 1, retain) == -1)
        {
            ESP_LOGE(TAG, "Client publish failed.");
            return ESP_FAIL;
        }

        ESP_LOGI(TAG, "Client publish success."); 
        return ESP_OK;
    }

    esp_err_t client::subscribe(std::string_view topic)
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        assert(m_client_handle);

        if (esp_mqtt_client_subscribe(m_client_handle, topic.data(), 0) == -1)
        {
            ESP_LOGE(TAG, "Client subscribe failed.");
            return ESP_FAIL;
        }

        ESP_LOGI(TAG, "Client subscribes to topic: %s.", topic.data());
        return ESP_OK;
    }

    esp_err_t client::unsubscribe(std::string_view topic)
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        assert(m_client_handle);

        if (esp_mqtt_client_unsubscribe(m_client_handle, topic.data()) == -1)
        {
            ESP_LOGE(TAG, "Client unsubscribe failed.");
            return ESP_FAIL;
        }

        ESP_LOGI(TAG, "Client unsubscribed from topic: %s.", topic.data());
        return ESP_OK;
    }

    void client::set_data_event_handler(event::data_event_handler_fun_t event_handler)
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        assert(m_client_handle);

        esp_mqtt_client_register_event(
            m_client_handle, 
            static_cast<esp_mqtt_event_id_t>(MQTT_EVENT_DATA), 
            &mqtt_event_handler,
            this);

        m_data_event_handler += event_handler;
    }
}