#ifndef DEVICE_MIKETTLE_H
#define DEVICE_MIKETTLE_H

#include "stdint.h"
#include "esp_err.h"
#include "hub_ble.h"
#include "hub_device.h"

#include <array>

namespace hub
{
    class MiKettle : public device_base
    {
        static constexpr uint8_t KEY_LENGTH     { 4 };
        static constexpr uint8_t TOKEN_LENGTH   { 12 };
        static constexpr uint16_t PERM_LENGTH   { 256 };

        static constexpr uint16_t PRODUCT_ID    { 275 };

        static constexpr std::array<uint8_t, KEY_LENGTH> key1       { 0x90, 0xCA, 0x85, 0xDE };
        static constexpr std::array<uint8_t, KEY_LENGTH> key2       { 0x92, 0xAB, 0x54, 0xFA };
        static constexpr std::array<uint8_t, TOKEN_LENGTH> token    { 0x91, 0xf5, 0x80, 0x92, 0x24, 0x49, 0xb4, 0x0d, 0x6b, 0x06, 0xd2, 0x8a };
        static constexpr uint8_t subscribe[]                        { 0x01, 0x00 };

        /* MiKettle services */
        static constexpr uint16_t GATT_UUID_KETTLE_SRV          { 0xfe95 };
        static constexpr uint16_t GATT_UUID_KETTLE_DATA_SRV[]   { 0x56, 0x61, 0x23, 0x37, 0x28, 0x26, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x36, 0x47, 0x34, 0x01 };

        /* MiKettle characteristics */
        static constexpr uint16_t GATT_UUID_AUTH_INIT   { 0x0010 };
        static constexpr uint16_t GATT_UUID_AUTH        { 0x0001 };
        static constexpr uint16_t GATT_UUID_VERSION     { 0x0004 };
        static constexpr uint16_t GATT_UUID_SETUP       { 0xaa01 };
        static constexpr uint16_t GATT_UUID_STATUS      { 0xaa02 };
        static constexpr uint16_t GATT_UUID_TIME        { 0xaa04 };
        static constexpr uint16_t GATT_UUID_BOIL_MODE   { 0xaa05 };
        static constexpr uint16_t GATT_UUID_MCU_VERSION { 0x2a28 };

        /* MiKettle handles */
        static constexpr uint8_t HANDLE_READ_FIRMWARE_VERSION   { 26 };
        static constexpr uint8_t HANDLE_READ_NAME               { 20 };
        static constexpr uint8_t HANDLE_AUTH_INIT               { 44 };
        static constexpr uint8_t HANDLE_AUTH                    { 37 };
        static constexpr uint8_t HANDLE_VERSION                 { 42 };
        static constexpr uint8_t HANDLE_KEEP_WARM               { 58 };
        static constexpr uint8_t HANDLE_STATUS                  { 61 };
        static constexpr uint8_t HANDLE_TIME_LIMIT              { 65 };
        static constexpr uint8_t HANDLE_BOIL_MODE               { 68 };

        static constexpr esp_bt_uuid_t uuid_service_kettle{
            ESP_UUID_LEN_16,
            {
                GATT_UUID_KETTLE_SRV,
            },
        };

        static void cipher_init(const uint8_t* const in_first1, const uint8_t* const in_last1, uint8_t* const out_first);

        static void cipher_crypt(const uint8_t* const in_first1, uint8_t* const in_first2, uint8_t* const out_first);

        static void cipher(const uint8_t* const in_first1, const uint8_t* const in_last1, const uint8_t* const in_first2, uint8_t* const out_first);

        static void mix_a(const uint8_t* in_first, const uint16_t product_id, uint8_t* out_first);

        static void mix_b(const uint8_t* in_first, const uint16_t product_id, uint8_t* out_first);

        union data_model
        {
            struct 
            {
                uint8_t action;
                uint8_t mode;
                uint16_t unknown;
                uint8_t temperature_set;
                uint8_t temperature_current;
                uint8_t keep_warm_type;
                uint16_t keep_warm_time;
            } data_struct;
            uint8_t data_array[sizeof(data_struct)];
        };

        data_model last_notify;

        volatile bool auth_notify;

        esp_err_t authorize(const ble::mac& address);

        esp_err_t wait_for_authorization();

    public:

        static constexpr const char* TAG                { "MiKettle" };
        static constexpr std::string_view device_name   { TAG };

        MiKettle()  = default;
        ~MiKettle() = default;

        std::string_view get_device_name() const override
        {
            return device_name;
        }

        esp_err_t connect(const ble::mac& address) override;

        esp_err_t disconnect() override;

        esp_err_t send_data(std::string_view data) override;

        void data_received(const uint16_t char_handle, std::string_view data) override;

        void device_disconnected() override;
    };
}

#endif