#include "mqtt/client.hpp"

namespace hub::mqtt
{
    namespace impl
    {
        client_state::client_state(const config_t& config) :
            m_handle{ nullptr },
            m_subject{  }
        {
            using namespace rxcpp::operators;

            esp_err_t result = ESP_OK;

            if (m_handle = esp_mqtt_client_init(&config); !m_handle)
            {
                LOG_AND_THROW(TAG, utils::esp_exception("MQTT client initialization failed."));
            }

            ESP_LOGD(TAG, "MQTT client initialized successfully.");

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

                        if (!mqtt_client)
                        {
                            return;
                        }

                        auto topic  = std::string_view(event_data->topic, event_data->topic_len);
                        auto data   = std::string_view(event_data->data, event_data->data_len);

                        mqtt_client->m_subject.get_subscriber().on_next(mqtt_message_t{ topic, data });
                    }
                    else if (event_data->event_id == MQTT_EVENT_DELETED)
                    {
                        ESP_LOGV(TAG, "MQTT event: MQTT_EVENT_DELETED.");

                        client_state* mqtt_client = reinterpret_cast<client_state*>(handler_args);

                        if (!mqtt_client)
                        {
                            return;
                        }

                        mqtt_client->m_subject.get_subscriber().on_completed();
                    }
                    else if (event_data->event_id == MQTT_EVENT_ERROR)
                    {
                        ESP_LOGV(TAG, "MQTT event: MQTT_EVENT_ERROR.");

                        client_state* mqtt_client = reinterpret_cast<client_state*>(handler_args);

                        if (!mqtt_client)
                        {
                            return;
                        }

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

    rxcpp::observable<client::message_t> client::subscribe(std::string_view topic, qos_t qos) noexcept
    {
        using namespace rxcpp::operators;

        esp_err_t result = esp_mqtt_client_subscribe(m_state->get_handle(), topic.data(), static_cast<int>(qos));
        if (result == ESP_FAIL)
        {
            ESP_LOGE(TAG, "MQTT client topic subscribe failed with error code: 0x%04x.", result);
            return rxcpp::observable<>::error<message_t>(utils::esp_exception("Could not subscribe to MQTT topic."));
        }

        ESP_LOGD(TAG, "MQTT client subscribed to topic: %.*s.", topic.length(), topic.data());

        return 
            m_state->get_observable() |
            tap([local_state{ m_state }](auto) {  }) |
            finally([topic, qos, handle{ m_state->get_handle() }]() {
                if (esp_err_t result = esp_mqtt_client_unsubscribe(handle, topic.data()) == ESP_FAIL)
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
            });
    }

    tl::expected<client, esp_err_t> make_client(std::string_view uri) noexcept
    {
        esp_mqtt_client_config_t config{  };

        config.uri = uri.data();

        try
        {
            return client{ std::make_shared<impl::client_state>(config) };
        }
        catch(const utils::esp_exception& err)
        {
            return tl::make_unexpected<esp_err_t>(err.errc());
        }
    }

    tl::expected<client, esp_err_t> make_client(const config_t& config) noexcept
    {
        try
        {
            return client{ std::make_shared<impl::client_state>(config) };
        }
        catch(const utils::esp_exception& err)
        {
            return tl::make_unexpected<esp_err_t>(err.errc());
        }
    }
}