#ifndef HUB_CONNECTED_HPP
#define HUB_CONNECTED_HPP

#include <string_view>
#include <functional>

#include "tl/expected.hpp"

#include "ble/scanner.hpp"

#include "configuration.hpp"

namespace hub
{
    class connected_t
    {
    public:

        connected_t(const configuration& config);

        connected_t()                               = delete;

        connected_t(const connected_t&)             = delete;

        connected_t(connected_t&&)                  = default;

        connected_t& operator=(const connected_t&)  = delete;

        connected_t& operator=(connected_t&&)       = default;

        ~connected_t()                              = default;

        tl::expected<void, esp_err_t> process_events() const noexcept;

    private:

        static constexpr const char* TAG{ "hub::app::connected_t" };

        std::reference_wrapper<const configuration> m_config;

        static std::string_view scan_result_to_string(ble::scanner::message_type message) noexcept;
    };
}

#endif