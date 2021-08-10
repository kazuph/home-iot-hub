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
        struct
        {
            std::string     ssid;
            std::string     password;
        } wifi;
        
        struct
        {
            std::string     uri;
        } mqtt;

        struct
        {
            std::string     name;
            std::string     object_id;
            std::string     discovery_prefix;
        } general;
    };
}

#endif