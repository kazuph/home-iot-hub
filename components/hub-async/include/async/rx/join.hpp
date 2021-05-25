#ifndef HUB_ASYNC_RX_JOIN_HPP
#define HUB_ASYNC_RX_JOIN_HPP

#include "traits.hpp"

#include <list>
#include <functional>
#include <type_traits>

namespace hub::async::rx
{
    namespace impl
    {
        template<typename SenderT>
        class join
        {
        public:

            static_assert(is_valid_source_v<SenderT>, "SenderT is not a valid message source.");
            static_assert(is_valid_source_v<typename SenderT::out_message_t>, "SenderT::out_message_t is not a valid message source.");

            using tag               = tag::two_way_tag;
            using in_message_t      = typename SenderT::out_message_t;
            using out_message_t     = typename in_message_t::out_message_t;
            using message_handler_t = std::function<void(out_message_t&&)>;

            join(SenderT&& sender) :
                m_sender(std::move(sender))
            {

            }

            template<typename MessageHandlerT>
            void set_message_handler(MessageHandlerT message_handler)
            {
                m_message_handler = message_handler;

                m_sender.set_message_handler([this](in_message_t&& message) {
                    process_message(std::move(message));
                });
            }

            void process_message(in_message_t&& message)
            {
                m_source_list.emplace_back(std::move(message));
                m_source_list.back().set_message_handler(m_message_handler);
            }

        private:

            SenderT                 m_sender;
            message_handler_t       m_message_handler;
            std::list<in_message_t> m_source_list;
        };

        struct join_helper{};
    }

    template<typename SenderT>
    inline auto join(SenderT&& sender)
    {
        return impl::join<SenderT>(std::forward<SenderT>(sender));
    }

    namespace operators
    {
        inline auto join()
        {
            return impl::join_helper();
        }

        template<typename SenderT>
        inline auto operator|(SenderT&& sender, impl::join_helper)
        {
            return impl::join<SenderT>(std::forward<SenderT>(sender));
        }
    }
}

#endif