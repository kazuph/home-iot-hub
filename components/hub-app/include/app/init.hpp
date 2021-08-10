#ifndef HUB_INIT_HPP
#define HUB_INIT_HPP

#include <string_view>
#include <functional>

#include "esp_err.h"

#include "tl/expected.hpp"

#include "configuration.hpp"

namespace hub
{
    class init_t
    {
    public:

        init_t()                            = default;

        init_t(const init_t&)               = delete;

        init_t(init_t&&)                    = default;

        init_t& operator=(const init_t&)    = delete;

        init_t& operator=(init_t&&)         = default;

        ~init_t()                           = default;

        tl::expected<std::reference_wrapper<init_t>, esp_err_t> initialize_filesystem() noexcept;

        tl::expected<std::reference_wrapper<init_t>, esp_err_t> initialize_ble() noexcept;

        tl::expected<std::reference_wrapper<init_t>, esp_err_t> connect_to_wifi(const configuration& config) noexcept;

        tl::expected<configuration, esp_err_t> read_config(std::string_view path) const noexcept;

        tl::expected<std::reference_wrapper<init_t>, esp_err_t> send_mqtt_config(const configuration& config) noexcept;

    private:

        static constexpr const char* TAG{ "hub::app::init_t" };
    };
}

#endif