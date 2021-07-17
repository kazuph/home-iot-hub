#ifndef HUB_CONFIGURATION_HPP
#define HUB_CONFIGURATION_HPP

#include <string>
#include <string_view>

#include "utils/mac.hpp"

#include "operations.hpp"

namespace hub
{
    struct configuration
    {
        static constexpr std::string_view SCAN_ENABLE_TOPIC    { "home/scan_enable" };
        static constexpr std::string_view SCAN_RESULTS_TOPIC   { "home/scan_results" };
        static constexpr std::string_view CONNECT_TOPIC        { "home/connect" };
        static constexpr std::string_view DISCONNECT_TOPIC     { "home/disconnect" };

        struct
        {
            std::string     ssid;
            std::string     password;
            utils::mac      mac;
        } wifi;
        
        struct
        {
            std::string     uri;
        } mqtt;

        struct
        {
            std::string     id;
            std::string     device_name;
        } hub;
    };
}

#endif