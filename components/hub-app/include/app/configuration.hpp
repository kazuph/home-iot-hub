#ifndef HUB_CONFIGURATION_HPP
#define HUB_CONFIGURATION_HPP

#include <string>

#include "utils/mac.hpp"

namespace hub
{
    struct configuration
    {
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