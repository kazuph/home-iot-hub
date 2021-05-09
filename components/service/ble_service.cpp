#include "ble_service.hpp"

namespace hub::service
{
    ble_service::ble_service()
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        ble::init();
        ble::scanner::init();
    }

    ble_service::~ble_service()
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        ble::scanner::deinit();
        ble::deinit();
    }

    void ble_service::process_message(in_message_t&& message) const
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        if (std::holds_alternative<scan_control_t>(message))
        {
            scan_control_handler(std::move(std::get<scan_control_t>(message)));
        }
        else if (std::holds_alternative<device_connect_t>(message))
        {
            device_connect_handler(std::move(std::get<device_connect_t>(message)));
        }
        else if (std::holds_alternative<device_disconnect_t>(message))
        {
            device_disconnect_handler(std::move(std::get<device_disconnect_t>(message)));
        }
        else if (std::holds_alternative<device_update_t>(message))
        {
            device_update_handler(std::move(std::get<device_update_t>(message)));
        }
    }

    void ble_service::scan_control_handler(scan_control_t&& scan_control_args) const
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        if (scan_control_args.m_enable)
        {
            ble::scanner::start();
        }
        else 
        {
            ble::scanner::stop();
        }
    }

    void ble_service::device_connect_handler(device_connect_t&& device_connect_args) const
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        //ble::client::make_client(device_connect_args.m_id)->connect(device_connect_args.m_address);
    }

    void ble_service::device_disconnect_handler(device_disconnect_t&& device_disconnect_args) const
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        //ble::client::get_client_by_id(device_disconnect_args.m_id)->disconnect();
    }

    void ble_service::device_update_handler(device_update_t&& device_update_args) const
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
    }
}