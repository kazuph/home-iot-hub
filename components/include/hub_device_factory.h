#ifndef HUB_DEVICE_LIST_H
#define HUB_DEVICE_LIST_H

#include "hub_const_map.h"
#include "hub_device.h"

#include <memory>
#include <string_view>

namespace hub::device_factory
{
    /*
        Create device instance.
    */
    std::unique_ptr<device_base> make_device(std::string_view device_name, std::string_view id);

    /*
        Check if device is supported.
    */
    bool supported(std::string_view device_name);

    /*
        Convert std::string_view on temporary string to another
        std::string_view on constexpr one.
    */
    std::string_view view_on_name(std::string_view device_name);
}

#endif