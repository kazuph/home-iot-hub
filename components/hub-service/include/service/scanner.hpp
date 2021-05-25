#ifndef HUB_SERVICE_SCANNER
#define HUB_SERVICE_SCANNER

#include "async/rx/traits.hpp"
#include "ble/mac.hpp"
#include "ble/scanner.hpp"

#include "esp_log.h"

#include <functional>
#include <string>
#include <type_traits>

namespace hub::service
{
    namespace impl
    {
        template<typename SenderT = void>
        class ble_scanner
        {
        public:

            static_assert(async::rx::is_valid_source_v<SenderT>, "Sender is not a valid message source.");

            using tag               = async::rx::tag::two_way_tag;
            using in_message_t      = typename SenderT::out_message_t;
            using out_message_t     = hub::event::scan_result_event_args;
            using message_handler_t = std::function<void(out_message_t&&)>;

            ble_scanner()                               = delete;

            ble_scanner(SenderT&& sender) :
                m_sender(std::move(sender))
            {

            }

            ble_scanner(const ble_scanner&)             = delete;

            ble_scanner(ble_scanner&&)                  = default;

            ble_scanner& operator=(const ble_scanner&)  = delete;

            ble_scanner& operator=(ble_scanner&&)       = default;

            ~ble_scanner() = default;

            template<typename MessageHandlerT>
            void set_message_handler(MessageHandlerT message_handler)
            {
                ESP_LOGD(TAG, "Function: %s.", __func__);

                hub::ble::scanner::set_scan_results_event_handler([message_handler](event::scan_result_event_args&& message) {
                    ESP_LOGD(TAG, "Function: scan_results_event_handler.");
                    message_handler({ message.m_name, message.m_address });
                });

                m_sender.set_message_handler([this](in_message_t&& message) {
                    process_message(std::move(message));
                });
            }

            void process_message(in_message_t&& message) const
            {
                ESP_LOGD(TAG, "Function: %s.", __func__);

                if (message.m_enable)
                {
                    hub::ble::scanner::start();
                }
                else 
                {
                    hub::ble::scanner::stop();
                }
            }

        private:

            static constexpr const char* TAG{ "hub::service::scanner" };

            SenderT             m_sender;
        };

        template<>
        class ble_scanner<void>
        {
        public:

            struct in_message_t
            {
                bool m_enable;
            };

            using out_message_t     = hub::event::scan_result_event_args;
            using tag               = async::rx::tag::two_way_tag;
            using message_handler_t = std::function<void(out_message_t&&)>;

            ble_scanner()                               = default;

            ble_scanner(const ble_scanner&)             = delete;

            ble_scanner(ble_scanner&&)                  = default;

            ble_scanner& operator=(const ble_scanner&)  = delete;

            ble_scanner& operator=(ble_scanner&&)       = default;

            ~ble_scanner() = default;

            template<typename MessageHandlerT>
            void set_message_handler(MessageHandlerT message_handler)
            {
                ESP_LOGD(TAG, "Function: %s.", __func__);

                hub::ble::scanner::set_scan_results_event_handler([message_handler](const event::scan_result_event_args& message) {
                    ESP_LOGD(TAG, "Function: scan_results_event_handler.");
                    message_handler({ message.m_name, message.m_address });
                });
            }

            void process_message(in_message_t&& message) const
            {
                ESP_LOGD(TAG, "Function: %s.", __func__);

                if (message.m_enable)
                {
                    hub::ble::scanner::start();
                }
                else 
                {
                    hub::ble::scanner::stop();
                }
            }

        private:

            static constexpr const char* TAG{ "hub::service::ble_scanner" };
        };

        static_assert(async::rx::is_valid_two_way_v<ble_scanner<>>, "ble_scanner is not a valid two way service.");

        struct ble_scanner_helper
        {

        };
    }

    namespace operators
    {
        inline auto ble_scanner() noexcept
        {
            return impl::ble_scanner_helper();
        }

        template<typename SenderT>
        inline auto operator|(SenderT&& sender, impl::ble_scanner_helper) noexcept
        {
            return impl::ble_scanner<SenderT>(std::forward<SenderT>(sender));
        }
    }

    using ble_scanner = impl::ble_scanner<>;
}

#endif