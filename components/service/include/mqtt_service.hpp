#ifndef MQTT_SERVICE_HPP
#define MQTT_SERVICE_HPP

#include "service_traits.hpp"
#include "mqtt.hpp"

#include <string>
#include <string_view>
#include <functional>

namespace hub::service
{
    /**
     * @brief MQTT actor class. Represents MQTT communication endpoint.
     * Class acts as both source and sink of the messages.
     */
    class mqtt_service
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

        mqtt_service() = delete;

        mqtt_service(mqtt_service&&) = default;

        mqtt_service(const mqtt_service&) = delete;

        mqtt_service(std::string_view uri);

        void process_message(in_message_t&& message) const;

        template<typename MessageHandlerT>
        void set_message_handler(MessageHandlerT message_handler)
        {
            m_message_handler = message_handler;

            m_client.set_data_event_handler([this](auto args) {
                std::invoke(m_message_handler, out_message_t{ args.topic, args.data });
            });
        }

    private:

        mutable mqtt::client    m_client;
        message_handler_t       m_message_handler;
    };

    static_assert(is_valid_two_way_service_v<mqtt_service>, "mqtt_service is not a valid two way service.");
}

#endif