#ifndef HUB_APP_CONSTS_HPP
#define HUB_APP_CONSTS_HPP

#include <string_view>

namespace hub
{
    inline constexpr std::string_view CONFIG_FILE_PATH{ "/spiffs/config.json" };

    inline constexpr std::string_view SENSOR_DEVICE_NAME{ "sensor" };

    inline constexpr std::string_view SWITCH_DEVICE_NAME{ "switch" };

    inline constexpr std::string_view TOPIC_PREFIX_FMT{ "{0}/{1}/{2}" };

    inline constexpr std::string_view SENSOR_CONFIG_PAYLOAD_FMT{
        "{{\"~\":\"{0}\",\"name\":\"{1}\",\"stat_t\":\"~/state\"}}"
    };

    inline constexpr std::string_view SWITCH_CONFIG_PAYLOAD_FMT{
        "{{\"~\":\"{0}\",\"name\":\"{1}\",\"stat_t\":\"~/state\",\"cmd_t\":\"~/set\"}}"
    };
}

#endif