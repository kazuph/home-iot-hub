#ifndef HUB_SERVICE_SCANNER
#define HUB_SERVICE_SCANNER

#include "service/traits.hpp"
#include "ble/mac.hpp"

#include "esp_log.h"

#include <functional>
#include <string>

namespace hub::service
{
    class scanner
    {
    public:

        struct in_message_t
        {
            bool m_enable;
        };

        struct out_message_t
        {
            std::string m_device_name;
            std::string m_device_address;
        };

        using service_tag       = service_tag::two_way_service_tag;
        using message_handler_t = std::function<void(out_message_t&&)>;

        scanner();

        scanner(const scanner&)             = delete;

        scanner(scanner&&)                  = default;

        scanner& operator=(const scanner&)  = delete;

        scanner& operator=(scanner&&)       = default;

        ~scanner();

        void process_message(in_message_t&& message) const;

        template<typename MessageHandlerT>
        void set_message_handler(MessageHandlerT message_handler)
        {
            ESP_LOGD(TAG, "Function: %s.", __func__);

            m_message_handler = message_handler;
        }

    private:

        static constexpr const char* TAG{ "hub::service::scanner" };

        message_handler_t m_message_handler;
    };

    static_assert(is_valid_two_way_service_v<scanner>, "scanner is not a valid two way service.");
}

#endif