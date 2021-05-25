#ifndef HUB_BLE_EVENT_HPP
#define HUB_BLE_EVENT_HPP

#include "event/event.hpp"

#include <vector>
#include <cstdint>

namespace hub::ble::event
{
    using notify_event_args_t       = std::vector<uint8_t>;
    using notify_event_handler_t    = hub::event::event_handler<notify_event_args_t>;
    using notify_function_t         = notify_event_handler_t::function_type;
}

namespace hub::event
{
    using namespace hub::ble::event;
}

#endif