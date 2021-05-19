#ifndef HUB_DEVICE_XIAOMI_MIKETTLE_HPP
#define HUB_DEVICE_XIAOMI_MIKETTLE_HPP

#include "utils/json.hpp"
#include "ble/client.hpp"

#include <memory>
#include <string_view>
#include <array>
#include <algorithm>

namespace hub::device::xiaomi
{
    class mikettle
    {
    public:

        static constexpr std::string_view device_name{ "Xiaomi MiKettle" };

        static void on_after_connect(std::shared_ptr<ble::client> client);

        static utils::json device_data_to_json(const ble::event::notify_event_args_t& device_data);

        static void send_data_to_device(utils::json&& data);

    private:

        static constexpr uint8_t KEY_LENGTH                         { 4 };
        static constexpr uint8_t TOKEN_LENGTH                       { 12 };
        static constexpr uint16_t PERM_LENGTH                       { 256 };

        static constexpr uint16_t PRODUCT_ID                        { 275 };

        static constexpr std::array<uint8_t, KEY_LENGTH> key1       { 0x90, 0xCA, 0x85, 0xDE };
        static constexpr std::array<uint8_t, KEY_LENGTH> key2       { 0x92, 0xAB, 0x54, 0xFA };
        static constexpr std::array<uint8_t, TOKEN_LENGTH> token    { 0x91, 0xf5, 0x80, 0x92, 0x24, 0x49, 0xb4, 0x0d, 0x6b, 0x06, 0xd2, 0x8a };
        static constexpr std::array<uint8_t, 2> subscribe           { 0x01, 0x00 };

        /* MiKettle services */
        static constexpr esp_bt_uuid_t GATT_UUID_KETTLE_SRV  { ESP_UUID_LEN_16, { 0xfe95 } };
        static constexpr esp_bt_uuid_t GATT_UUID_KETTLE_DATA_SRV{ 
            .len = ESP_UUID_LEN_128, 
            .uuid = { .uuid128 = { 0x56, 0x61, 0x23, 0x37, 0x28, 0x26, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x36, 0x47, 0x34, 0x01 } } 
        };

        /* MiKettle characteristics */
        static constexpr esp_bt_uuid_t GATT_UUID_AUTH_INIT  { ESP_UUID_LEN_16, { 0x0010 } };
        static constexpr esp_bt_uuid_t GATT_UUID_AUTH       { ESP_UUID_LEN_16, { 0x0001 } };
        static constexpr esp_bt_uuid_t GATT_UUID_VERSION    { ESP_UUID_LEN_16, { 0x0004 } };
        static constexpr esp_bt_uuid_t GATT_UUID_SETUP      { ESP_UUID_LEN_16, { 0xaa01 } };
        static constexpr esp_bt_uuid_t GATT_UUID_STATUS     { ESP_UUID_LEN_16, { 0xaa02 } };
        static constexpr esp_bt_uuid_t GATT_UUID_TIME       { ESP_UUID_LEN_16, { 0xaa04 } };
        static constexpr esp_bt_uuid_t GATT_UUID_BOIL_MODE  { ESP_UUID_LEN_16, { 0xaa05 } };
        static constexpr esp_bt_uuid_t GATT_UUID_MCU_VERSION{ ESP_UUID_LEN_16, { 0x2a28 } };

        static void cipher_init(const uint8_t* const in_first1, const uint8_t* const in_last1, uint8_t* const out_first)
        {
            const uint16_t key_size{ static_cast<uint16_t>(std::distance(in_first1, in_last1)) };
            uint16_t j{ 0 };

            for (uint16_t i{ 0 }; i < PERM_LENGTH; i++)
            {
                out_first[i] = i;
            }

            for (uint16_t i{ 0 }; i < PERM_LENGTH; i++)
            {
                j += out_first[i] + in_first1[i % key_size];
                j &= 0xff;
                std::swap(out_first[i], out_first[j]);
            }
        }

        static void cipher_crypt(const uint8_t* const in_first1, uint8_t* const in_first2, uint8_t* const out_first)
        {
            uint16_t index1{ 0 };
            uint16_t index2{ 0 };
            uint16_t index3{ 0 };

            for (uint16_t i{ 0 }; i < TOKEN_LENGTH; i++)
            {
                index1++;
                index1 &= 0xff;
                index2 += in_first2[index1];
                index2 &= 0xff;
                std::swap(in_first2[index1], in_first2[index2]);
                index3 = in_first2[index1] + in_first2[index2];
                index3 &= 0xff;
                out_first[i] = in_first1[i] ^ in_first2[index3];
            }
        }

        static void cipher(const uint8_t* const in_first1, const uint8_t* const in_last1, const uint8_t* const in_first2, uint8_t* const out_first)
        {
            std::vector<uint8_t> perm(PERM_LENGTH);

            cipher_init(in_first1, in_last1, perm.data());
            cipher_crypt(in_first2, perm.data(), out_first);
        }

        static void mix_a(const uint8_t* in_first, const uint16_t product_id, uint8_t* out_first)
        {
            out_first[0] = in_first[0];
            out_first[1] = in_first[2];
            out_first[2] = in_first[5];
            out_first[3] = product_id & 0xff;
            out_first[4] = product_id & 0xff;
            out_first[5] = in_first[4];
            out_first[6] = in_first[5];
            out_first[7] = in_first[1];
        }

        static void mix_b(const uint8_t* in_first, const uint16_t product_id, uint8_t* out_first)
        {
            out_first[0] = in_first[0];
            out_first[1] = in_first[2];
            out_first[2] = in_first[5];
            out_first[3] = (product_id >> 8) & 0xff;
            out_first[4] = in_first[4];
            out_first[5] = in_first[0];
            out_first[6] = in_first[5];
            out_first[7] = product_id & 0xff;
        }
    };
}

#endif