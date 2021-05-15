#include "service/mqtt.hpp"

#include "esp_err.h"

namespace hub::service
{
    mqtt::mqtt(std::string_view uri) :
        m_client    {  }
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        if (esp_err_t result = m_client.connect(uri); result != ESP_OK)
        {
            throw std::runtime_error("Could not connect to MQTT client.");
        }

        m_client.subscribe(MQTT_BLE_SCAN_ENABLE_TOPIC);
        m_client.subscribe(MQTT_BLE_CONNECT_TOPIC);
        m_client.subscribe(MQTT_BLE_DISCONNECT_TOPIC);
        m_client.subscribe(MQTT_BLE_DEVICE_WRITE_TOPIC);
    }

    void mqtt::process_message(in_message_t&& message) const
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        
        m_client.publish(message.m_topic, message.m_data);
    }
}