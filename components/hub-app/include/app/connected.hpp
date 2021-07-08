#ifndef HUB_CONNECTED_HPP
#define HUB_CONNECTED_HPP

#include "configuration.hpp"

#include <string_view>
#include <memory>

#include "tl/expected.hpp"

#include "rxcpp/rx.hpp"

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

        tl::expected<void, esp_err_t> process_events() const
        {
            using result_type = tl::expected<void, esp_err_t>;

            using namespace std::literals;
            using namespace rx::operators;

            constexpr auto SCAN_ENABLE_TOPIC = "home/scan_enable";
            constexpr auto SCAN_RESULTS_TOPIC = "home/scan_results";

            auto mqtt_client = mqtt::make_client(config.get().mqtt.uri);

            return result_type();
        }

    private:

        std::reference_wrapper<const configuration> m_config;
    };
}

#endif