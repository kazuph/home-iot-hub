#ifndef HUB_NOT_CONNECTED_HPP
#define HUB_NOT_CONNECTED_HPP

#include <functional>
#include <string>
#include <memory>

#include "esp_err.h"

#include "tl/expected.hpp"

#include "configuration.hpp"

namespace hub
{
    class not_connected_t 
    {
    public:

        not_connected_t(const configuration &config);

        not_connected_t()                                       = delete;

        not_connected_t(const not_connected_t &)                = delete;

        not_connected_t(not_connected_t &&)                     = default;

        not_connected_t &operator=(const not_connected_t &)     = delete;

        not_connected_t &operator=(not_connected_t &&)          = default;

        ~not_connected_t()                                      = default;

        tl::expected<std::string, esp_err_t> wait_for_id_assignment() const noexcept;

    private:

        std::reference_wrapper<const configuration> m_config;

        static constexpr const char *TAG{ "hub::not_connected_t" };

        static std::string retrieve_id(std::shared_ptr<rapidjson::Document> message_ptr);
    };
}

#endif