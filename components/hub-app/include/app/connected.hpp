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
#include "mqtt/client.hpp"
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

        tl::expected<void, esp_err_t> process_events() const noexcept
        {
            using result_type = tl::expected<void, esp_err_t>;

            using namespace std::literals;
            using namespace rxcpp::operators;

            auto mqtt_client         = mqtt::make_client(m_config.get().mqtt.uri).value();   // Factory of observables and subscribers. Abort on error.
            auto ble_scanner_factory = ble::scanner::get_observable_factory();               // Each scan is handled by an individual observable

            /* 
                Stream handling MQTT messages published on SCAN_ENABLE_TOPIC.. Passes the "enable" signal to the ble::scanner,
                creating new observables. Scan messages from these observables are then gathered and
                published via MQTT on SCAN_RESULTS_TOPIC.
            */
            mqtt_client.get_observable(m_config.get().SCAN_ENABLE_TOPIC) |
            filter(&is_empty_sv) |
            map([ble_scanner_factory](mqtt::client::message_t) { 
                return ble_scanner_factory(3 /* seconds */); 
            }) |
            switch_on_next() |
            map(&scan_result_to_string) |
            subscribe<mqtt::client::message_t>(
                mqtt_client.get_subscriber(m_config.get().SCAN_RESULTS_TOPIC));


            /*
                Disconnect pipeline blocks the current thread untill disconnect request is published
                on DISCONNECT_TOPIC.
            */
            auto disconnect_message =
                mqtt_client.get_observable(m_config.get().DISCONNECT_TOPIC) |
                map(&parse_json) |
                filter(&is_parse_success) |
                filter(&is_disconnect_message) |
                filter(disconnect_message_predicate(
                    m_config.get().hub.device_name, 
                    m_config.get().hub.id, 
                    m_config.get().wifi.mac)) |
                take(1) |
                as_blocking();

            disconnect_message.first();

            return result_type();
        }

    private:

        static constexpr const char* TAG{ "hub::app::connected_t" };

        std::reference_wrapper<const configuration> m_config;

        inline static std::string_view scan_result_to_string(ble::scanner::message_type message) noexcept
        {
            thread_local std::string cache;

            rapidjson::Document json;
            auto &allocator = json.GetAllocator();
            json.SetObject();

            json.AddMember("name",    rapidjson::StringRef(message.name.data(), message.name.length()), allocator);
            json.AddMember("address", rapidjson::StringRef(message.mac.data(), message.mac.length()),   allocator);

            cache = utils::json::dump(std::move(json));

            return std::string_view(cache);
        }
    };
}

#endif