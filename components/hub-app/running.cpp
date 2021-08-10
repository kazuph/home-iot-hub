#include <memory>
#include <array>

#include "esp_err.h"
#include "esp_log.h"

#include "rxcpp/rx.hpp"

#include "fmt/format.h"
#include "rapidjson/document.h"

#include "mqtt/client.hpp"
#include "utils/json.hpp"

#include "app/consts.hpp"
#include "app/running.hpp"

namespace hub
{
    running_t::running_t(const configuration& config) :
        m_config{ std::cref(config) }
    {

    }

    tl::expected<void, esp_err_t> running_t::run() const noexcept
    {
        using namespace std::literals;

        namespace rx = rxcpp;
        using namespace rx::operators;

        const auto make_topic = [](std::string_view topic_prefix, std::string_view topic_postfix) {
            constexpr std::string_view topic_fmt{ "{0}/{1}" };
            return fmt::format(topic_fmt, topic_prefix, topic_postfix);
        };

        const auto& config = m_config.get();

        const auto switch_topic_prefix = fmt::format(
            TOPIC_PREFIX_FMT, 
            config.general.discovery_prefix,
            SWITCH_DEVICE_NAME,
            config.general.object_id);

        const auto sensor_topic_prefix = fmt::format(
            TOPIC_PREFIX_FMT, 
            config.general.discovery_prefix,
            SENSOR_DEVICE_NAME,
            config.general.object_id);

        auto mqtt_client = mqtt::make_client(config.mqtt.uri)
            .or_else([](esp_err_t err) {
                ESP_LOGE(TAG, "MQTT client initialization failed.");
            })
            .value();  // Factory of observables and subscribers. Abort on error.

        std::string switch_command_topic = make_topic(switch_topic_prefix, "set");
        std::string sensor_state_topic   = make_topic(sensor_topic_prefix, "state");

        auto ble_scan_results = mqtt_client.subscribe(switch_command_topic) |
            filter([](std::string_view message) {
                return message == "ON";
            }) |
            map([make_ble_scanner{ ble::scanner::get_observable_factory() }](std::string_view) { 
                return make_ble_scanner(3); 
            }) |
            switch_on_next() |
            map([cache{ std::string() }] (ble::scanner::message_type message) mutable {
                rjs::Document json;
                auto& allocator = json.GetAllocator();

                json.SetObject();
                json.AddMember("name", rjs::StringRef(message.name.data(), message.name.length()), allocator);
                json.AddMember("address", rjs::StringRef(message.mac.data(), message.mac.length()), allocator);

                cache = utils::json::dump(std::move(json));
                return std::string_view(cache);
            }) |
            mqtt_client.publish(sensor_state_topic) |
            as_blocking();

        ble_scan_results.subscribe(
            [](int) { return; }, // ignore message id
            [](const std::exception_ptr& err) {
                try
                {
                    std::rethrow_exception(err);
                }
                catch(const utils::esp_exception& ex)
                {
                    ESP_LOGE(TAG, "%s", ex.what());
                }       
            }
        );

        return tl::expected<void, esp_err_t>();
    }
}
