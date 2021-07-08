#ifndef HUB_NOT_CONNECTED_HPP
#define HUB_NOT_CONNECTED_HPP

#include <cstring>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string_view>

#include "tl/expected.hpp"

#include "rxcpp/rx.hpp"

#include "rapidjson/document.h"
#include "rapidjson/rapidjson.h"
#include "rapidjson/memorystream.h"

#include "timing/timing.hpp"

#include "configuration.hpp"
#include "service/mqtt_client.hpp"
#include "utils/esp_exception.hpp"
#include "utils/json.hpp"

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

        tl::expected<std::string, esp_err_t> wait_for_id_assignment() const noexcept
        {
            using namespace std::literals;
            using namespace rxcpp::operators;

            namespace mqtt = service::mqtt;

            constexpr std::string_view SCAN_TOPIC{ "home/scan" };
            constexpr std::string_view CONNECT_TOPIC{ "home/connect" };

            const auto is_connect_message = [this](std::shared_ptr<rapidjson::Document> message_ptr) {
                const auto &message = *message_ptr;
                const auto &config = m_config.get();
                return (
                    message.IsObject() &&
                    message.HasMember("name") &&
                    message.HasMember("address") &&
                    message.HasMember("id") &&
                    message["name"].IsString() &&
                    message["address"].IsString() &&
                    message["id"].IsString() &&
                    (config.hub.device_name == message["name"].GetString()) &&
                    (std::string(config.wifi.mac) == message["address"].GetString()));
            };

            mqtt::client mqtt_client;

            try
            {
                mqtt_client = mqtt::make_client(m_config.get().mqtt.uri);
            }
            catch (const utils::esp_exception &err)
            {
                return tl::make_unexpected<esp_err_t>(err.errc());
            }

            {
                std::string scan_message;

                {
                    rapidjson::Document json;
                    auto &allocator = json.GetAllocator();
                    json.SetObject();

                    json.AddMember("device_name", rapidjson::StringRef(m_config.get().hub.device_name), allocator);
                    json.AddMember("device_address", rapidjson::Value().SetString(std::string(m_config.get().wifi.mac), allocator), allocator);

                    scan_message = utils::json::dump(std::move(json));
                }

                rxcpp::observable<>::from<std::string_view>(scan_message) |
                    subscribe<std::string_view>(mqtt_client.get_subscriber(SCAN_TOPIC, mqtt::client::qos_t::exactly_once));
            }

            auto connect_message =
                mqtt_client.get_observable(CONNECT_TOPIC) |
                map(parse_json) |
                filter(is_parse_success) |
                filter(is_connect_message) |
                map(retrieve_id) |
                take(1) |
                as_blocking();

            return connect_message.first();
        }

    private:
        std::reference_wrapper<const configuration> m_config;

        static constexpr const char *TAG{ "hub::not_connected_t" };

        inline static std::shared_ptr<rapidjson::Document> parse_json(std::string_view message) noexcept
        {
            auto result = std::make_shared<rapidjson::Document>();
            result->Parse(message.data(), message.length());
            return result;
        }

        inline static bool is_parse_success(std::shared_ptr<rapidjson::Document> message_ptr) noexcept
        {
            return !(message_ptr->HasParseError());
        }

        inline static std::string retrieve_id(std::shared_ptr<rapidjson::Document> message_ptr)
        {
            const auto& message = *message_ptr;
            return std::string(message["id"].GetString(), message["id"].GetStringLength());
        }
    };
}

#endif