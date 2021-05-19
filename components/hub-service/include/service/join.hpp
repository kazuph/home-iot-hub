#ifndef HUB_SERVICE_JOIN_HPP
#define HUB_SERVICE_JOIN_HPP

#include "traits.hpp"

#include <list>
#include <functional>
#include <type_traits>

namespace hub::service
{
    namespace impl
    {
        template<typename SenderT>
        class join
        {
        public:

            using in_message_t  = typename SenderT::out_message_t;
            using out_message_t = typename in_message_t::out_message_t;
            using function_type = std::function<void(out_message_t&&)>;

            static_assert(has_output_message_type_v<SenderT>, "SenderT is not a valid message source.");
            static_assert(has_output_message_type_v<in_message_t>, "in_message_t is not a valid message source.");

            join(SenderT&& sender) :
                m_sender(std::move(sender))
            {

            }

            template<typename MessageHandlerT>
            void set_message_handler(MessageHandlerT&& message_handler)
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
            function_type           m_message_handler;
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