#include "service/mqtt_client.hpp"

#include "utils/esp_exception.hpp"

#include "esp_log.h"

namespace hub::service::mqtt
{
    namespace impl
    {
        client_state::client_state(std::string_view uri) :
            m_handle{ nullptr },
            m_subject{  }
        {
            using namespace rxcpp::operators;

            esp_err_t result = ESP_OK;
            esp_mqtt_client_config_t mqtt_client_config{  };

            if (m_handle = esp_mqtt_client_init(&mqtt_client_config); !m_handle)
            {
                LOG_AND_THROW(TAG, utils::esp_exception("MQTT client initialization failed."));
            }

            ESP_LOGD(TAG, "MQTT client initialized successfully.");

            if (result = esp_mqtt_client_set_uri(m_handle, "mqtt://192.168.0.109:1883"); result != ESP_OK)
            {
                LOG_AND_THROW(TAG, utils::esp_exception("Unable to set MQTT URI."));
            }

            ESP_LOGD(TAG, "MQTT URI: \"%.*s\" set successfully.", uri.length(), uri.data());

            result = esp_mqtt_client_register_event(
                m_handle,
                static_cast<esp_mqtt_event_id_t>(ESP_EVENT_ANY_ID),
                [](void *handler_args, esp_event_base_t base, int32_t event_id, void* event) {

                    esp_mqtt_event_handle_t event_data = reinterpret_cast<esp_mqtt_event_handle_t>(event);

                    ESP_LOGV(TAG, "MQTT event code: %i.", event_data->event_id);

                    if (event_data->event_id == MQTT_EVENT_DATA)
                    {
                        ESP_LOGV(TAG, "MQTT event: MQTT_EVENT_DATA.");

                        client_state* mqtt_client = reinterpret_cast<client_state*>(handler_args);

                        auto topic = std::string_view(event_data->topic, event_data->topic_len);
                        auto data = std::string_view(event_data->data, event_data->data_len);

                        mqtt_client->m_subject.get_subscriber().on_next(mqtt_message_t{ topic, data });
                    }
                    else if (event_data->event_id == MQTT_EVENT_DELETED)
                    {
                        ESP_LOGV(TAG, "MQTT event: MQTT_EVENT_DELETED.");

                        client_state* mqtt_client = reinterpret_cast<client_state*>(handler_args);

                        mqtt_client->m_subject.get_subscriber().on_completed();
                    }
                    else if (event_data->event_id == MQTT_EVENT_ERROR)
                    {
                        ESP_LOGV(TAG, "MQTT event: MQTT_EVENT_ERROR.");

                        client_state* mqtt_client = reinterpret_cast<client_state*>(handler_args);

                        mqtt_client->m_subject.get_subscriber().on_error(
                            std::make_exception_ptr(utils::esp_exception("MQTT error occured."))
                        );
                    }
                },
                this
            );

            if (result != ESP_OK)
            {
                LOG_AND_THROW(TAG, utils::esp_exception("Event registration failed.", result));
            }

            ESP_LOGD(TAG, "MQTT events registration success.");

            if (result = esp_mqtt_client_start(m_handle); result != ESP_OK)
            {
                LOG_AND_THROW(TAG, utils::esp_exception("Could not start MQTT client.", result));
            }

            ESP_LOGD(TAG, "MQTT client start success.");
            ESP_LOGI(TAG, "MQTT client state initialization success.");
        }

        client_state::~client_state()
        {
            esp_err_t result = ESP_OK;

            if (!m_handle)
            {
                return;
            }

            if (result = esp_mqtt_client_stop(m_handle); result != ESP_OK)
            {
                ESP_LOGW(TAG, "MQTT client stop failed with error code: 0x%04x.", result);
            }

            if (result = esp_mqtt_client_destroy(m_handle); result != ESP_OK)
            {
                ESP_LOGW(TAG, "MQTT client handle destroy failed with error code: 0x%04x.", result);
                return;
            }

            ESP_LOGI(TAG, "MQTT client state destruction success.");
        }
    }

    rxcpp::observable<client::message_t> client::get_observable(std::string_view topic, qos_t qos) noexcept
    {
        using namespace rxcpp::operators;

        auto local_state = m_state;

        esp_err_t result = esp_mqtt_client_subscribe(local_state->get_handle(), topic.data(), static_cast<int>(qos));
        if (result == ESP_FAIL)
        {
            ESP_LOGE(TAG, "MQTT client topic subscribe failed with error code: 0x%04x.", result);

            return rxcpp::observable<>::error<message_t>(utils::esp_exception("Could not subscribe to MQTT topic."));
        }

        ESP_LOGD(TAG, "MQTT client subscribed to topic: %.*s.", topic.length(), topic.data());

        return 
            m_state->get_observable() |
            finally([=]() {
                if (esp_err_t result = esp_mqtt_client_unsubscribe(local_state->get_handle(), topic.data()) == ESP_FAIL)
                {
                    LOG_AND_THROW(TAG, utils::esp_exception("Unable to unsubscribe from topic.", result));
                }

                ESP_LOGD(TAG, "Unsubscribed from topic: %.*s.", topic.length(), topic.data());
                ESP_LOGD(TAG, "MQTT topic observable finalized.");
            }) |
            filter(topic_predicate(topic)) |
            map([](impl::client_state::mqtt_message_t message) {
                ESP_LOGV(TAG, "Received data: %.*s.", message.data.length(), message.data.data());
                return message.data; 
            }) |
            publish() |
            ref_count();
    }

    rxcpp::subscriber<client::message_t> client::get_subscriber(std::string_view topic, qos_t qos, bool retain)
    {
        using namespace rxcpp::operators;

        auto local_state = m_state;

        return rxcpp::make_subscriber<message_t>(
            [=](message_t message) {
                ESP_LOGD(TAG, "Publishing on topic: %.*s.", topic.length(), topic.data());
                esp_err_t result = esp_mqtt_client_publish(
                    local_state->get_handle(), 
                    topic.data(), 
                    message.data(), 
                    message.length(), 
                    static_cast<int>(qos),
                    static_cast<int>(retain));

                if (result == ESP_FAIL)
                {
                    ESP_LOGE(TAG, "Data publish failed with error code: 0x%04x.", result);
                    return;
                }

                ESP_LOGV(TAG, "Published data: %.*s.", message.length(), message.data());
            }
        );
    }
}