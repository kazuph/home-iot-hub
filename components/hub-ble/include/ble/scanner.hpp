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
        struct scan_result_event_args
        {
            std::string m_name;
            std::string m_address;
        };

        using scan_result_event_handler_t       = hub::event::event_handler<scan_result_event_args>;
        using scan_result_event_handler_fun_t   = scan_result_event_handler_t::function_type;
    }

    void init();

    void deinit();

    void start(uint16_t duration = 3);

    void stop();

    void set_scan_results_event_handler(event::scan_result_event_handler_fun_t scan_callback);
}

namespace hub::event
{
    using namespace hub::ble::scanner::event;
}

#endif