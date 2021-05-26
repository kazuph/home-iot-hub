#ifndef HUB_BLE_SCANNER_HPP
#define HUB_BLE_SCANNER_HPP

#include "errc.hpp"
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

    result<void> init() noexcept;

    result<void> deinit() noexcept;

    result<void> start(uint16_t duration = 3) noexcept;

    result<void> stop() noexcept;

    result<void> set_scan_results_event_handler(event::scan_result_event_handler_fun_t scan_callback) noexcept;
}

namespace hub::event
{
    using namespace hub::ble::scanner::event;
}

#endif