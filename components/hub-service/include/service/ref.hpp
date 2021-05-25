#ifndef HUB_SERVICE_REF_HPP
#define HUB_SERVICE_REF_HPP

#include "async/rx/traits.hpp"

#include "esp_log.h"

#include <type_traits>
#include <functional>

namespace hub::service
{
    /**
     * @brief Class representing reference to the service classes. 
     * It is needed when service object is to be used in more than one pipeline.
     * Allows copying and moving.
     * 
     * @tparam ServiceT *_service type.
     */
    template<typename ServiceT>
    class ref
    {
    public:

        static_assert(async::rx::is_valid_v<ServiceT>, "Not a valid service.");

        using tag               = typename ServiceT::tag;
        using in_message_t      = std::enable_if_t<async::rx::has_input_message_type_v<ServiceT>, typename ServiceT::in_message_t>;
        using out_message_t     = std::enable_if_t<async::rx::has_output_message_type_v<ServiceT>, typename ServiceT::out_message_t>;
        using message_handler_t = std::enable_if_t<async::rx::has_message_handler_type_v<ServiceT>, typename ServiceT::message_handler_t>;

        ref() = delete;

        ref(ref&&) = default;

        ref(const ref&) = default;

        ref(ServiceT& service) :
            m_service{ std::ref(service) }
        {
            ESP_LOGD(TAG, "Function: %s.", __func__);
        }

        /**
         * @brief Passes message to ServiceT::process_message.
         * 
         * @tparam std::enable_if_t<async::rx::has_sink_tag_v<ServiceT> || async::rx::has_two_way_tag_v<ServiceT>, void> Enabled when service can operate as sink.
         * @param message rvalue reference to the message to be processed.
         */
        template<typename = std::enable_if_t<async::rx::has_sink_tag_v<ServiceT> || async::rx::has_two_way_tag_v<ServiceT>, void>>
        void process_message(in_message_t&& message) const
        {
            ESP_LOGD(TAG, "Function: %s.", __func__);
            m_service.get().process_message(std::move(message));
        }

        /**
         * @brief Sets message handler in ServiceT.
         * 
         * @tparam MessageHandlerT Type of the function receiving ServiceT::out_message_t as parameter.
         * @tparam std::enable_if_t<async::rx::has_source_tag_v<ServiceT> || async::rx::has_two_way_tag_v<ServiceT>, void> Enabled when service can operate as source.
         * @param message_handler Message handler of type MessageHandlerT.
         */
        template<typename MessageHandlerT, typename = std::enable_if_t<async::rx::has_source_tag_v<ServiceT> || async::rx::has_two_way_tag_v<ServiceT>, void>>
        void set_message_handler(MessageHandlerT&& message_handler)
        {
            ESP_LOGD(TAG, "Function: %s.", __func__);
            m_service.get().set_message_handler(std::forward<MessageHandlerT>(message_handler));
        }

    private:
    
        static constexpr const char* TAG{ "hub::service::ref" };

        std::reference_wrapper<ServiceT> m_service;
    };
}

#endif