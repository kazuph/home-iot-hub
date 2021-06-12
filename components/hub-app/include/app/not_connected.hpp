#ifndef HUB_NOT_CONNECTED_HPP
#define HUB_NOT_CONNECTED_HPP

#include <cstring>
#include <memory>
#include <stdexcept>
#include <functional>

#include "rxcpp/rx.hpp"

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

            constexpr auto topic_predicate = [](std::string_view topic) {
                return [=](const event::data_event_args& message) {
                    return message.topic == topic;
                };
            };

            const auto parse_json = [](const event::data_event_args& message) {
                std::shared_ptr<rapidjson::Document> result;
                result->Parse(std::move(message.data));

                if (result->HasParseError())
                {
                    throw std::runtime_error("Parsing failed.");
                }

                return result;
            };

            const auto is_connect_message = [this](std::shared_ptr<rapidjson::Document> message_ptr) {
                const auto& message = *message_ptr;
                return (
                    message.IsObject()                                                      && 
                    message.HasMember("device_name")                                        && 
                    message.HasMember("device_address")                                     && 
                    message.HasMember("device_id")                                          &&
                    message["device_name"].IsString()                                       &&
                    message["device_address"].IsString()                                    &&
                    message["device_id"].IsString()                                         &&
                    (std::string_view(message["device_name"].GetString()) == "HubBLE"sv)    &&
                    (std::string_view(message["device_address"].GetString()) == m_config->wifi.mac.to_string()));
            };

            const auto retrieve_id([](std::shared_ptr<rapidjson::Document> message_ptr) {
                const auto& message = *message_ptr;
                return std::string(message["device_id"].GetString(), message["device_id"].GetStringLength());
            });

            auto mqtt_client = mqtt::client();
            mqtt_client.connect(m_config->mqtt.uri);

            mqtt_client.subscribe("home/connect");

            auto mqtt_observable = rx::observable<>::create<event::data_event_args>(
                [&, this](rx::subscriber<event::data_event_args> subscriber) {
                    mqtt_client.set_data_event_handler([=](event::data_event_args&& data) {
                        subscriber.on_next(std::move(data));
                    });
                }
            );

            auto connect_message = 
                mqtt_observable                         |
                filter(topic_predicate("home/connect")) | 
                map(parse_json)                         | 
                filter(is_connect_message)              |
                first()                                 |
                map(retrieve_id)                        |
                as_blocking();

            {
                std::string result;
                connect_message.subscribe([&result](std::string msg) { 
                    result = std::move(msg);
                });
                return result;
            }
        }

    private:

        std::reference_wrapper<const configuration> m_config;
    };
}

#endif