#ifndef HUB_SERVICE_SINK_HPP
#define HUB_SERVICE_SINK_HPP

#include "traits.hpp"

namespace hub::service
{
    namespace impl
    {
        template<typename SenderT, typename SinkT>
        class sink
        {
        public:

            using service_tag = service_tag::sink_service_tag;
            using in_message_t = typename SenderT::out_message_t;

            static_assert(std::is_invocable_v<SinkT, in_message_t>, "Provided sink is not invocable with the given sender message type.");

            sink() = delete;

            sink(SenderT&& sender, SinkT sink) :
                m_sender(std::move(sender)),
                m_sink(sink)
            {
                m_sender.set_message_handler([this](in_message_t&& message) {
                    process_message(std::move(message));
                });
            }

            void process_message(in_message_t&& message) const
            {
                std::invoke(m_sink, std::move(message));
            }

        private:

            SenderT m_sender;
            SinkT   m_sink;
        };

        template<typename SinkT>
        struct sink_helper
        {
            SinkT m_sink;
        };
    }

    template<typename SenderT, typename SinkT>
    inline auto sink(SenderT&& sender, SinkT&& sink)
    {
        return impl::sink<SenderT, SinkT>(std::forward<SenderT>(sender), std::forward<SinkT>(sink));
    }

    namespace operators
    {
        template<typename SinkT>
        inline auto sink(SinkT&& sink)
        {
            return impl::sink_helper<SinkT>{ std::forward<SinkT>(sink) };
        }

        template<typename SenderT, typename SinkT>
        inline auto operator|(SenderT&& sender, impl::sink_helper<SinkT> sink)
        {
            return impl::sink<SenderT, SinkT>(std::forward<SenderT>(sender), sink.m_sink);
        }
    }
}

#endif