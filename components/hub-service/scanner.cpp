#include "service/scanner.hpp"

namespace hub::service
{
    scanner::scanner()
    {
        ble::scanner::init();
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