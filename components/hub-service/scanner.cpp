#include "service/scanner.hpp"

#include "ble/scanner.hpp"

namespace hub::service
{
    scanner::scanner() :
        m_message_handler{}
    {
        ble::scanner::init();

        ble::scanner::set_scan_results_event_handler([this](const ble::scanner::event::scan_results_event_args& message) {
            if (!m_message_handler)
            {
                return;
            }

            m_message_handler({ message.m_address.to_string(), message.m_name });
        });
    }

    scanner::~scanner()
    {
        ble::scanner::deinit();
    }

    void scanner::process_message(in_message_t&& message) const
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        if (message.m_enable)
        {
            hub::ble::scanner::start();
        }
        else 
        {
            hub::ble::scanner::stop();
        }
    }
}