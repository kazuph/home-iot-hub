#ifndef SERVICE_FILTER_HPP
#define SERVICE_FILTER_HPP

#include "service_traits.hpp"

#include "esp_log.h"

#include <type_traits>
#include <functional>

namespace hub::service
{
    namespace impl
    {
        template<typename SenderT, typename PredT>
        class filter
        {
        public:

            using in_message_t  = typename SenderT::out_message_t;
            using out_message_t = in_message_t;
            using function_type = std::function<void(out_message_t&&)>;

            static_assert(has_output_message_type_v<SenderT>, "Sender is not a valid message source.");
            static_assert(std::is_invocable_v<PredT, in_message_t>, "Specified callable is not invocable with sender message type.");
            static_assert(std::is_same_v<std::invoke_result_t<PredT, in_message_t>, bool>, "Not a valid predicate.");

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

            static constexpr const char* TAG{ "FILTER" };

            SenderT         m_sender;
            PredT           m_predicate;
            function_type   m_message_handler;
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