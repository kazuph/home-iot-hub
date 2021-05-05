#ifndef SINK_OPERATOR_HPP
#define SINK_OPERATOR_HPP

#include "service_traits.hpp"

namespace hub::service::operators
{
    template<typename SenderT, typename SinkT>
    inline auto operator|(SenderT&& sender, SinkT&& sink)
    {
        static_assert(has_output_message_type_v<SenderT>, "Not a valid source.");
        static_assert(is_valid_sink_service_v<SinkT>, "Not a valid sink.");

        sender.set_message_handler([sink{ std::forward<SinkT>(sink) }](auto&& message) {
            sink.process_message(std::move(message));
        });

        return sender;
    }
}

#endif