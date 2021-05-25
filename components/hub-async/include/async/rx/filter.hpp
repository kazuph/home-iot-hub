#ifndef HUB_SERVICE_FILTER_HPP
#define HUB_SERVICE_FILTER_HPP

#include "traits.hpp"

#include "esp_log.h"

#include <type_traits>
#include <functional>

namespace hub::async::rx
{
    namespace impl
    {
        template<typename SenderT, typename PredT>
        class filter
        {
        public:

            static_assert(is_valid_source_v<SenderT>, "Sender is not a valid message source.");
            static_assert(std::is_invocable_v<PredT, typename SenderT::out_message_t>, "Specified callable is not invocable with sender output message type.");
            static_assert(std::is_same_v<std::invoke_result_t<PredT, typename SenderT::out_message_t>, bool>, "Not a valid predicate.");

            using tag               = tag::two_way_tag;
            using in_message_t      = typename SenderT::out_message_t;
            using out_message_t     = in_message_t;
            using message_handler_t = std::function<void(out_message_t&&)>;

            filter() = delete;

            filter(SenderT&& sender, PredT predicate) :
                m_sender(std::move(sender)),
                m_predicate(predicate)
            {
                ESP_LOGD(TAG, "Function: %s.", __func__);
            }

            template<typename MessageHandlerT>
            void set_message_handler(MessageHandlerT message_handler)
            {
                ESP_LOGD(TAG, "Function: %s.", __func__);

                m_message_handler = message_handler;

                m_sender.set_message_handler([this](in_message_t&& message) {
                    process_message(std::move(message));
                });
            }

            void process_message(in_message_t&& message) const
            {
                ESP_LOGD(TAG, "Function: %s.", __func__);

                if (m_message_handler && std::invoke(m_predicate, message))
                {
                    m_message_handler(std::move(message));
                }
            }

        private:

            static constexpr const char* TAG{ "hub::service::impl::filter" };

            SenderT             m_sender;
            PredT               m_predicate;
            message_handler_t   m_message_handler;
        };

        template<typename FunctionT>
        struct filter_helper
        {
            FunctionT m_function;
        };
    }

    template<typename SenderT, typename PredT>
    inline auto filter(SenderT&& sender, PredT&& predicate)
    {
        return impl::filter<SenderT, PredT>(std::forward<SenderT>(sender), std::forward<PredT>(predicate));
    }

    namespace operators
    {
        template<typename PredT>
        inline auto filter(PredT&& predicate)
        {
            return impl::filter_helper<PredT>{ std::forward<PredT>(predicate) };
        }

        template<typename SenderT, typename PredT>
        inline auto operator|(SenderT&& sender, impl::filter_helper<PredT> filter)
        {
            return impl::filter<SenderT, PredT>(std::forward<SenderT>(sender), filter.m_function);
        }
    }
}

#endif