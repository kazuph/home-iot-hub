#ifndef HUB_SERVICE_SINK_OPERATOR_HPP
#define HUB_SERVICE_SINK_OPERATOR_HPP

#include "traits.hpp"
#include "ref.hpp"

namespace hub::service::operators
{
    template<typename SenderT, typename SinkT>
    inline auto operator|(SenderT&& sender, SinkT&& sink)
    {
        static_assert(has_output_message_type_v<SenderT>, "Not a valid source.");
        static_assert(is_valid_sink_service_v<SinkT>, "Not a valid sink.");

        sender.set_message_handler([sink](auto&& message) {
            sink.process_message(std::move(message));
        });

        return sink;
    }
}

#endif