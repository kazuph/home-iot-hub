#ifndef HUB_SERVICE_SCANNER
#define HUB_SERVICE_SCANNER

#include "service/traits.hpp"
#include "ble/mac.hpp"
#include "ble/scanner.hpp"

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

        using out_message_t     = hub::event::scan_result_event_args;
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

            hub::ble::scanner::set_scan_results_event_handler([message_handler](const event::scan_result_event_args& message) {
                ESP_LOGD(TAG, "Function: scan_results_event_handler.");
                message_handler({ message.m_name, message.m_address });
            });
        }

    private:

        static constexpr const char* TAG{ "hub::service::scanner" };
    };

    static_assert(is_valid_two_way_service_v<scanner>, "scanner is not a valid two way service.");
}

#endif