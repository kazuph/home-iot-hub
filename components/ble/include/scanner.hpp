#ifndef SCANNER_HPP
#define SCANNER_HPP

#include "event.hpp"
#include "mac.hpp"

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