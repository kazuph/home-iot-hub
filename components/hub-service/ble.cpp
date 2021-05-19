#include "service/ble.hpp"

#include <stdexcept>

namespace hub::service
{
    ble_message_source::ble_message_source(std::weak_ptr<ble::client> client) :
        m_client{ client }
    {
        ESP_LOGD(TAG, "Function: %s.", __func__);

        if (m_client.expired())
        {
            throw std::invalid_argument("Client has disconnected.");
        }
    }
}