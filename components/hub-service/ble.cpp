#include "service/ble.hpp"

#include "ble/ble.hpp"
#include "ble/scanner.hpp"

namespace hub::service
{
    ble::ble()
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        hub::ble::init();
        hub::ble::scanner::init();

        hub::ble::scanner::set_scan_results_event_handler([this](const hub::ble::scanner::event::scan_results_event_args& message) -> void {
            if (!m_message_handler)
            {
                return;
            }

            m_message_handler(out_message_t(scan_result_t{ message.m_address, message.m_name }));
        });
    }

    ble::~ble()
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        hub::ble::scanner::deinit();
        hub::ble::deinit();
    }

    void ble::process_message(in_message_t&& message) const
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

    void ble::scan_control_handler(scan_control_t&& scan_control_args) const
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        if (scan_control_args.m_enable)
        {
            hub::ble::scanner::start();
        }
        else 
        {
            hub::ble::scanner::stop();
        }
    }

    void ble::device_connect_handler(device_connect_t&& device_connect_args) const
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        auto client = hub::ble::client::make_client(device_connect_args.m_id);

        client->connect(device_connect_args.m_address);

        client->set_notify_event_handler([this](const hub::ble::event::notify_event_args_t& message) -> void {
            return;
        });

        m_client_list.push_back(client);
    }

    void ble::device_disconnect_handler(device_disconnect_t&& device_disconnect_args) const
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
        //ble::client::get_client_by_id(device_disconnect_args.m_id)->disconnect();
    }

    void ble::device_update_handler(device_update_t&& device_update_args) const
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);
    }
}