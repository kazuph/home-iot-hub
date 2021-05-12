#ifndef HUB_BLE_SCANNER_HPP
#define HUB_BLE_SCANNER_HPP

#include "mac.hpp"

#include "event/event.hpp"

#include <cstdint>
#include <utility>
#include <string>

namespace hub::ble::scanner
{
    namespace event
    {
        struct scan_results_event_args
        {
            std::string m_name;
            mac         m_address;
        };

        using scan_results_event_handler_t  = hub::event::event_handler<scan_results_event_args>;
    }

    void init();

    void deinit();

    void start(uint16_t duration = 3);

    void stop();

    void set_scan_results_event_handler(event::scan_results_event_handler_t::function_type scan_callback);
}

#endif