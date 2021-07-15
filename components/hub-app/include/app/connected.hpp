#ifndef HUB_CONNECTED_HPP
#define HUB_CONNECTED_HPP

#include "configuration.hpp"

#include <string_view>
#include <memory>
#include <array>

#include "tl/expected.hpp"

#include "rxcpp/rx.hpp"
#include "rxcpp/rx-trace.hpp"

#include "rapidjson/document.h"

#include "ble/scanner.hpp"
#include "service/mqtt_client.hpp"
#include "utils/json.hpp"
#include "utils/result.hpp"

namespace hub
{
    class connected_t
    {
    public:

        connected_t(const configuration& config) :
            m_config{ std::cref(config) }
        {

        }

        connected_t()                               = delete;

        connected_t(const connected_t&)             = delete;

        connected_t(connected_t&&)                  = default;

        connected_t& operator=(const connected_t&)  = delete;

        connected_t& operator=(connected_t&&)       = default;

        ~connected_t()                              = default;

        tl::expected<void, esp_err_t> process_events() const
        {
            using result_type = tl::expected<void, esp_err_t>;

            using namespace std::literals;
            using namespace rxcpp::operators;

            constexpr auto SCAN_ENABLE_TOPIC    = "home/scan_enable"sv;
            constexpr auto SCAN_RESULTS_TOPIC   = "home/scan_results"sv;
            constexpr auto DISCONNECT_TOPIC     = "home/disconnect"sv;

            const auto is_disconnect_message = [this](std::shared_ptr<rapidjson::Document> message_ptr) {
                const auto &message = *message_ptr;
                const auto &config  = m_config.get();

                std::array<char, utils::mac::MAC_STR_SIZE> address_buff;
                config.wifi.mac.to_charbuff(address_buff.begin());
                std::string_view address{ address_buff.begin(), address_buff.size() };

                return
                    message.IsObject() &&
                    message.HasMember("name") &&
                    message.HasMember("address") &&
                    message.HasMember("id") &&
                    message["name"].IsString() &&
                    message["address"].IsString() &&
                    message["id"].IsString() &&
                    (config.hub.device_name == message["name"].GetString()) &&
                    (address == message["address"].GetString()) &&
                    (config.hub.id == message["id"].GetString());
            };

            auto mqtt_client         = service::mqtt::make_client(m_config.get().mqtt.uri);  // Factory of observables and subscribers.
            auto ble_scanner_factory = ble::scanner::get_observable_factory();               // Each scan is handled by an individual observable

            /* 
                Stream handling MQTT messages published on SCAN_ENABLE_TOPIC.. Passes the "enable" signal to the ble::scanner,
                creating new observables. Scan messages from these observables are then gathered and
                published via MQTT on SCAN_RESULTS_TOPIC.
            */
            mqtt_client.get_observable(SCAN_ENABLE_TOPIC) |
            filter(is_empty_sv) |
            map([ble_scanner_factory](service::mqtt::client::message_t) { 
                return ble_scanner_factory(3 /* seconds */); 
            }) |
            switch_on_next() |
            map(scan_result_to_string) |
            subscribe<service::mqtt::client::message_t>(mqtt_client.get_subscriber(SCAN_RESULTS_TOPIC));

            /*
                Disconnect pipeline blocks the current thread untill disconnect request is published
                on DISCONNECT_TOPIC.
            */
            auto disconnect_message =
                mqtt_client.get_observable(DISCONNECT_TOPIC) |
                map(parse_json) |
                filter(is_parse_success) |
                filter(is_disconnect_message) |
                take(1) |
                as_blocking();

            disconnect_message.first();

            return result_type();
        }

    private:

        static constexpr const char* TAG{ "hub::app::connected_t" };

        std::reference_wrapper<const configuration> m_config;

        inline static std::shared_ptr<rapidjson::Document> parse_json(std::string_view message) noexcept
        {
            auto result = std::make_shared<rapidjson::Document>();
            result->Parse(message.data(), message.length());
            return result;
        }

        inline static bool is_empty_sv(std::string_view str) noexcept
        {
            return str.length() == 0;
        }

        inline static bool is_parse_success(std::shared_ptr<rapidjson::Document> message_ptr) noexcept
        {
            return !(message_ptr->HasParseError());
        }

        inline static std::string_view scan_result_to_string(ble::scanner::message_type message) noexcept
        {
            thread_local std::string cache;

            rapidjson::Document json;
            auto &allocator = json.GetAllocator();
            json.SetObject();

            json.AddMember("name", rapidjson::StringRef(message.name.data(), message.name.length()), allocator);
            json.AddMember("address", rapidjson::StringRef(message.mac.data(), message.mac.length()), allocator);

            cache = utils::json::dump(std::move(json));

            return std::string_view(cache);
        }
    };
}

#endif