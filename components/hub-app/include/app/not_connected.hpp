#ifndef HUB_NOT_CONNECTED_HPP
#define HUB_NOT_CONNECTED_HPP

#include <cstring>
#include <memory>
#include <stdexcept>
#include <functional>
#include <string_view>

#include "rxcpp/rx.hpp"

#include "rapidjson/rapidjson.h"
#include "rapidjson/document.h"

#include "mqtt/client.hpp"
#include "timing/timing.hpp"

#include "configuration.hpp"

namespace hub
{
    template<typename T>
    struct error;

    class not_connected_t
    {
    public:

        not_connected_t()                                   = delete;

        not_connected_t(const configuration& config) :
            m_config{ std::cref(config) }
        {

        }

        not_connected_t(const not_connected_t&)             = delete;

        not_connected_t(not_connected_t&&)                  = default;

        not_connected_t& operator=(const not_connected_t&)  = delete;

        not_connected_t& operator=(not_connected_t&&)       = default;

        ~not_connected_t()                                  = default;

        std::string wait_for_id_assignment() const
        {
            using namespace std::literals;

            namespace rx = rxcpp;
            using namespace rx::operators;

            constexpr std::string_view CONNECT_TOPIC{ "home/connect" };

            constexpr auto topic_predicate = [](std::string_view topic) {
                return [=](const event::data_event_args& message) {
                    return message.topic == topic;
                };
            };

            const auto parse_json = [](const event::data_event_args& message) {
                std::shared_ptr<rapidjson::Document> result;
                result->Parse(std::move(message.data));
                return result;
            };

            const auto has_parse_error = [](std::shared_ptr<rapidjson::Document> message_ptr) {
                return !(message_ptr->HasParseError());
            };

            const auto is_connect_message = [this](std::shared_ptr<rapidjson::Document> message_ptr) {
                const auto& message = *message_ptr;
                return (
                    message.IsObject()                                                                  && 
                    message.HasMember("device_name")                                                    && 
                    message.HasMember("device_address")                                                 && 
                    message.HasMember("device_id")                                                      &&
                    message["device_name"].IsString()                                                   &&
                    message["device_address"].IsString()                                                &&
                    message["device_id"].IsString()                                                     &&
                    (std::string_view(message["device_name"].GetString()) == m_config.get().hub.device_name) &&
                    (std::string_view(message["device_address"].GetString()) == m_config.get().wifi.mac.to_string()));
            };

            const auto retrieve_id([](std::shared_ptr<rapidjson::Document> message_ptr) {
                const auto& message = *message_ptr;
                return std::string(message["device_id"].GetString(), message["device_id"].GetStringLength());
            });

            auto mqtt_client = mqtt::client();
            mqtt_client.connect(m_config.get().mqtt.uri);

            mqtt_client.subscribe(CONNECT_TOPIC);

            auto mqtt_observable = rx::observable<>::create<event::data_event_args>(
                [&, this](rx::subscriber<event::data_event_args> subscriber) {
                    mqtt_client.set_data_event_handler([=](event::data_event_args&& data) {
                        subscriber.on_next(std::move(data));
                    });
                }
            );

            auto connect_message = 
                mqtt_observable                         |
                filter(topic_predicate(CONNECT_TOPIC))  | 
                map(parse_json)                         | 
                filter(has_parse_error)                 |
                filter(is_connect_message)              |
                take(1)                                 |
                map(retrieve_id)                        |
                as_blocking();

            return connect_message.first();
        }

    private:

        std::reference_wrapper<const configuration> m_config;
    };
}

#endif