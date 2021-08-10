#ifndef HUB_RUNNING_HPP
#define HUB_RUNNING_HPP

#include <string_view>
#include <functional>

#include "tl/expected.hpp"

#include "ble/scanner.hpp"

#include "configuration.hpp"

namespace hub
{
    class running_t
    {
    public:

        running_t(const configuration& config);

        running_t()                                 = delete;

        running_t(const running_t&)                 = delete;

        running_t(running_t&&)                      = default;

        running_t& operator=(const running_t&)      = delete;

        running_t& operator=(running_t&&)           = default;

        ~running_t()                                = default;

        tl::expected<void, esp_err_t> run() const noexcept;

    private:

        static constexpr const char* TAG{ "hub::app::running_t" };

        std::reference_wrapper<const configuration> m_config;
    };
}

#endif