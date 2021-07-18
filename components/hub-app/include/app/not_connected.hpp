#ifndef HUB_NOT_CONNECTED_HPP
#define HUB_NOT_CONNECTED_HPP

#include <cstring>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <array>

#include "tl/expected.hpp"

#include "rxcpp/rx.hpp"
#include "rxcpp/rx-trace.hpp"

#include "rapidjson/document.h"
#include "rapidjson/rapidjson.h"
#include "rapidjson/memorystream.h"

#include "timing/timing.hpp"

#include "mqtt/client.hpp"
#include "utils/esp_exception.hpp"
#include "utils/json.hpp"

#include "configuration.hpp"
#include "operations.hpp"

namespace hub
{
    class not_connected_t 
    {
    public:

        not_connected_t(const configuration &config) 
            : m_config{ std::cref(config) }
        { }

        not_connected_t()                                       = delete;

        not_connected_t(const not_connected_t &)                = delete;

        not_connected_t(not_connected_t &&)                     = default;

        not_connected_t &operator=(const not_connected_t &)     = delete;

        not_connected_t &operator=(not_connected_t &&)          = default;

        ~not_connected_t()                                      = default;

        inline tl::expected<std::string, esp_err_t> wait_for_id_assignment() const noexcept
        {
            return mqtt::make_client(m_config.get().mqtt.uri)
                .map([this](mqtt::client&& mqtt_client) {
                    using namespace rxcpp::operators;

                    std::string scan_message;

                    {
                        std::array<char, utils::mac::MAC_STR_SIZE> mac_address;
                        m_config.get().wifi.mac.to_charbuff(mac_address.data());

                        rapidjson::Document json;
                        auto &allocator = json.GetAllocator();
                        json.SetObject();

                        json.AddMember("name",    rapidjson::StringRef(m_config.get().hub.device_name),         allocator);
                        json.AddMember("address", rapidjson::StringRef(mac_address.data(), mac_address.size()), allocator);

                        scan_message = utils::json::dump(std::move(json));
                    }

                    mqtt_client.get_observable(m_config.get().SCAN_ENABLE_TOPIC) |
                    filter(&is_empty_sv) |
                    map([&](std::string_view) { 
                        return std::string_view(scan_message);
                    }) |
                    subscribe<mqtt::client::message_t>(mqtt_client.get_subscriber(m_config.get().SCAN_RESULTS_TOPIC));

                    auto connect_message =
                        mqtt_client.get_observable(m_config.get().CONNECT_TOPIC) |
                        map(&parse_json) |
                        filter(&is_parse_success) |
                        filter(&is_connect_message) |
                        filter(connect_message_predicate(
                            m_config.get().hub.device_name, 
                            m_config.get().wifi.mac)) |
                        map(retrieve_id) |
                        take(1) |
                        as_blocking();

                    return connect_message.first();
                });

            /*mqtt::client mqtt_client;

            if (auto result = mqtt::make_client(m_config.get().mqtt.uri); !result.has_value())
            {
                return result.error();
            }
            else
            {
                mqtt_client = result.value();
            }

            {
                std::string scan_message;

                {
                    std::array<char, utils::mac::MAC_STR_SIZE> mac_address;
                    m_config.get().wifi.mac.to_charbuff(mac_address.data());

                    rapidjson::Document json;
                    auto &allocator = json.GetAllocator();
                    json.SetObject();

                    json.AddMember("name",    rapidjson::StringRef(m_config.get().hub.device_name),         allocator);
                    json.AddMember("address", rapidjson::StringRef(mac_address.data(), mac_address.size()), allocator);

                    scan_message = utils::json::dump(std::move(json));
                }

                mqtt_client.get_observable(m_config.get().SCAN_ENABLE_TOPIC) |
                filter(&is_empty_sv) |
                map([&](std::string_view) { 
                    return std::string_view(scan_message);
                }) |
                subscribe<mqtt::client::message_t>(mqtt_client.get_subscriber(m_config.get().SCAN_RESULTS_TOPIC));

                auto connect_message =
                    mqtt_client.get_observable(m_config.get().CONNECT_TOPIC) |
                    map(&parse_json) |
                    filter(&is_parse_success) |
                    filter(&is_connect_message) |
                    filter(connect_message_predicate(
                        m_config.get().hub.device_name, 
                        m_config.get().wifi.mac)) |
                    map(retrieve_id) |
                    take(1) |
                    as_blocking();

                return connect_message.first();
            }*/
        }

    private:

        std::reference_wrapper<const configuration> m_config;

        static constexpr const char *TAG{ "hub::not_connected_t" };

        inline static std::string retrieve_id(std::shared_ptr<rapidjson::Document> message_ptr)
        {
            const auto& message = *message_ptr;
            return std::string(message["id"].GetString(), message["id"].GetStringLength());
        }
    };
}

#endif