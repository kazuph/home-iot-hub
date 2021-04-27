#ifndef DESCRIPTOR_HPP
#define DESCRIPTOR_HPP

#include "client.hpp"

#include "esp_bt_defs.h"
#include "esp_gatt_defs.h"
#include "esp_gattc_api.h"

#include <memory>

namespace hub::ble
{
    class descriptor
    {
    public:

        descriptor() = delete;

        descriptor(std::shared_ptr<client> client_ptr, esp_gattc_descr_elem_t descriptor);

        void write(std::vector<uint8_t> data);

        std::vector<uint8_t> read() const;

    private:

        static constexpr const char* TAG{ "DESCRIPTOR" };

        esp_gattc_descr_elem_t  m_descriptor;
        std::shared_ptr<client> m_client_ptr;
    }
}

#endif