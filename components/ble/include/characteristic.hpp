#ifndef CHARACTERISTIC_H
#define CHARACTERISTIC_H

#include "client.hpp"
#include "descriptor.hpp"

#include "esp_bt_defs.h"
#include "esp_gatt_defs.h"
#include "esp_gattc_api.h"

#include <memory>

namespace hub::ble
{
    class characteristic
    {
    public:

        characteristic() = delete;

        characteristic(std::shared_ptr<client> client_ptr, esp_gattc_char_elem_t characteristic);

        void write(std::vector<uint8_t> data);

        std::vector<uint8_t> read() const;

        uint16_t get_handle() const noexcept;

        std::vector<descriptor> get_descriptors() const;

    private:

        static constexpr const char* TAG{ "CHARACTERISTIC" };

        esp_gattc_char_elem_t   m_characteristic;
        std::shared_ptr<client> m_client_ptr;
    }
}

#endif