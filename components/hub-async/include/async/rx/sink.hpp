#ifndef HUB_ASYNC_RX__SINK_HPP
#define HUB_ASYNC_RX__SINK_HPP

#include "traits.hpp"

namespace hub::async::rx
{
    namespace impl
    {
        template<typename SenderT, typename SinkT>
        class sink
        {
        public:

            static_assert(is_valid_source_v<SenderT>, "Sender is not a valid message source.");
            static_assert(std::is_invocable_v<SinkT, typename SenderT::out_message_t>, "Provided sink is not invocable with the given sender message type.");

            using tag          = tag::two_way_tag;
            using in_message_t = typename SenderT::out_message_t;

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
                m_sink(std::move(message));
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