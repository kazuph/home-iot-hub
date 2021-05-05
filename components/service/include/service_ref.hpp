#ifndef SERVICE_REF_HPP
#define SERVICE_REF_HPP

#include "service_traits.hpp"

#include <type_traits>
#include <functional>

namespace hub::service
{
    /**
     * @brief Class representing reference to the *_service classes. 
     * It is needed when service object is to be used in more than one pipeline.
     * Allows copying and moving.
     * 
     * @tparam ServiceT *_service type.
     */
    template<typename ServiceT>
    class service_ref
    {
    public:

        static_assert(is_valid_service_v<ServiceT>, "Not a valid service.");

        using service_tag       = typename ServiceT::service_tag;
        using in_message_t      = std::enable_if_t<has_input_message_type_v<ServiceT>, typename ServiceT::in_message_t>;
        using out_message_t     = std::enable_if_t<has_output_message_type_v<ServiceT>, typename ServiceT::out_message_t>;
        using message_handler_t = std::enable_if_t<has_message_handler_type_v<ServiceT>, typename ServiceT::message_handler_t>;

        service_ref() = delete;

        service_ref(service_ref&&) = default;

        service_ref(const service_ref&) = default;

        service_ref(ServiceT& service) :
            m_service{ std::ref(service) }
        {

        }

        /**
         * @brief Passes message to ServiceT::process_message.
         * 
         * @tparam std::enable_if_t<has_sink_service_tag_v<ServiceT> || has_two_way_service_tag_v<ServiceT>, void> Enabled when service can operate as sink.
         * @param message rvalue reference to the message to be processed.
         */
        template<typename = std::enable_if_t<has_sink_service_tag_v<ServiceT> || has_two_way_service_tag_v<ServiceT>, void>>
        void process_message(in_message_t&& message) const
        {
            m_service.get().process_message(std::move(message));
        }

        /**
         * @brief Sets message handler in ServiceT.
         * 
         * @tparam MessageHandlerT Type of the function receiving ServiceT::out_message_t as parameter.
         * @tparam std::enable_if_t<has_source_service_tag_v<ServiceT> || has_two_way_service_tag_v<ServiceT>, void> Enabled when service can operate as source.
         * @param message_handler Message handler of type MessageHandlerT.
         */
        template<typename MessageHandlerT, typename = std::enable_if_t<has_source_service_tag_v<ServiceT> || has_two_way_service_tag_v<ServiceT>, void>>
        void set_message_handler(MessageHandlerT message_handler)
        {
            m_service.get().set_message_handler(message_handler);
        }

    private:

        std::reference_wrapper<ServiceT> m_service;
    };
}

#endif