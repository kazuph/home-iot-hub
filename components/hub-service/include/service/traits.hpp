#ifndef HUB_SERVICE_TRAITS_HPP
#define HUB_SERVICE_TRAITS_HPP

#include <type_traits>

namespace hub::service
{
    namespace service_tag
    {
        struct source_service_tag {  };

        struct sink_service_tag {  };

        struct two_way_service_tag {  };
    }

    template<typename, typename = void>
    struct has_service_tag : std::false_type {  };

    template<typename T>
    struct has_service_tag<T, std::void_t<typename T::service_tag>> : std::true_type {  };

    template<typename, typename = void>
    struct has_input_message_type : std::false_type {  };

    template<typename T>
    struct has_input_message_type<T, std::void_t<typename T::in_message_t>> : std::true_type {  };

    template<typename, typename = void>
    struct has_output_message_type : std::false_type {  };

    template<typename T>
    struct has_output_message_type<T, std::void_t<typename T::out_message_t>> : std::true_type {  };

    template<typename, typename = void>
    struct has_message_handler_type : std::false_type {  };

    template<typename T>
    struct has_message_handler_type<T, std::void_t<typename T::message_handler_t>> : std::true_type {  };

    template<typename T>
    inline constexpr bool has_service_tag_v             = has_service_tag<T>::value;

    template<typename T>
    inline constexpr bool has_source_service_tag_v      = has_service_tag_v<T> && std::is_same_v<typename T::service_tag, service_tag::source_service_tag>;

    template<typename T>
    inline constexpr bool has_sink_service_tag_v        = has_service_tag_v<T> && std::is_same_v<typename T::service_tag, service_tag::sink_service_tag>;

    template<typename T>
    inline constexpr bool has_two_way_service_tag_v     = has_service_tag_v<T> && std::is_same_v<typename T::service_tag, service_tag::two_way_service_tag>;

    template<typename T>
    inline constexpr bool has_input_message_type_v      = has_input_message_type<T>::value;

    template<typename T>
    inline constexpr bool has_output_message_type_v     = has_output_message_type<T>::value;

    template<typename T>
    inline constexpr bool has_message_handler_type_v    = has_message_handler_type<T>::value;

    template<typename T>
    inline constexpr bool is_valid_source_service_v     = (has_source_service_tag_v<T> || has_two_way_service_tag_v<T>) && has_output_message_type_v<T> && has_message_handler_type_v<T>;

    template<typename T>
    inline constexpr bool is_valid_sink_service_v       = (has_sink_service_tag_v<T> || has_two_way_service_tag_v<T>) && has_input_message_type_v<T>;

    template<typename T>
    inline constexpr bool is_valid_two_way_service_v    = has_two_way_service_tag_v<T> && has_input_message_type_v<T> && has_output_message_type_v<T> && has_message_handler_type_v<T>;

    template<typename T>
    inline constexpr bool is_valid_service_v            = is_valid_source_service_v<T> || is_valid_sink_service_v<T> || is_valid_two_way_service_v<T>;
}

#endif