#ifndef HUB_SERVICE_MQTT_HPP
#define HUB_SERVICE_MQTT_HPP

#include "traits.hpp"

#include "mqtt/client.hpp"

#include "esp_log.h"

#include <string>
#include <string_view>
#include <functional>

namespace hub::service
{
    /**
     * @brief MQTT actor class. Represents MQTT communication endpoint.
     * Class acts as both source and sink of the messages.
     */
    class mqtt
    {
    public:

        static constexpr std::string_view MQTT_BLE_SCAN_ENABLE_TOPIC        { "/scan/enable" };
        static constexpr std::string_view MQTT_BLE_SCAN_RESULTS_TOPIC       { "/scan/results" };
        static constexpr std::string_view MQTT_BLE_CONNECT_TOPIC            { "/connect" };
        static constexpr std::string_view MQTT_BLE_DISCONNECT_TOPIC         { "/disconnect" };
        static constexpr std::string_view MQTT_BLE_DEVICE_READ_TOPIC        { "/device/read" };
        static constexpr std::string_view MQTT_BLE_DEVICE_WRITE_TOPIC       { "/device/write" };

        struct message_t
        {
            std::string m_topic;
            std::string m_data;
        };

        using service_tag       = service_tag::two_way_service_tag;
        using in_message_t      = message_t;
        using out_message_t     = message_t;
        using message_handler_t = std::function<void(out_message_t&&)>;

        mqtt() = delete;

        mqtt(mqtt&&) = default;

        mqtt(const mqtt&) = delete;

        mqtt(std::string_view uri);

        void process_message(in_message_t&& message) const;

        template<typename MessageHandlerT>
        void set_message_handler(MessageHandlerT&& message_handler)
        {
            ESP_LOGD(TAG, "Function: %s.", __func__);

            m_client.set_data_event_handler([message_handler{ std::forward<MessageHandlerT>(message_handler) }](auto args) {
                std::invoke(message_handler, out_message_t{ args.topic, args.data });
            });
        }

    private:

        static constexpr const char* TAG{ "hub::service::mqtt" };

        mutable hub::mqtt::client m_client;
    };

    static_assert(is_valid_two_way_service_v<mqtt>, "mqtt is not a valid two way service.");
}

#endif