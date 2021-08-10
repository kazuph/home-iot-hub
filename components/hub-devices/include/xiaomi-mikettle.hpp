#ifndef HUB_DEVICE_XIAOMI_MIKETTLE_HPP
#define HUB_DEVICE_XIAOMI_MIKETTLE_HPP

#include "device_base.hpp"

#include "utils/json.hpp"
#include "ble/client.hpp"
#include "utils/mac.hpp"

#include <memory>
#include <string_view>
#include <array>
#include <algorithm>
#include <vector>
#include <numeric>

namespace hub::device::xiaomi
{
    class mikettle final : public device_base
    {
    public:

        static constexpr std::string_view DEVICE_NAME{ "MiKettle" };

        void connect(utils::mac address)                override;

        void disconnect()                               override;

        void process_message(in_message_t&& message)    override;

    private:

        static constexpr const char* TAG{ "hub::device::xiaomi::mikettle" };

        static constexpr uint8_t    KEY_LENGTH                      { 4 };
        static constexpr uint8_t    TOKEN_LENGTH                    { 12 };
        static constexpr uint16_t   PERM_LENGTH                     { 256 };

        static constexpr uint16_t   PRODUCT_ID                      { 275 };

        static constexpr std::array<uint8_t, KEY_LENGTH> key1       { 0x90, 0xCA, 0x85, 0xDE };
        static constexpr std::array<uint8_t, KEY_LENGTH> key2       { 0x92, 0xAB, 0x54, 0xFA };
        static constexpr std::array<uint8_t, TOKEN_LENGTH> token    { 0x91, 0xf5, 0x80, 0x93, 0x24, 0x49, 0xb4, 0x0d, 0x6b, 0x06, 0xd2, 0x8a };
        static constexpr std::array<uint8_t, 2> subscribe           { 0x01, 0x00 };

        /* MiKettle services */
        static constexpr esp_bt_uuid_t GATT_UUID_KETTLE_SRV         { ESP_UUID_LEN_16, { 0xfe95 } };
        static constexpr esp_bt_uuid_t GATT_UUID_KETTLE_DATA_SRV{ 
            .len    = ESP_UUID_LEN_128, 
            .uuid   = { .uuid128 = { 0x56, 0x61, 0x23, 0x37, 0x28, 0x26, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x36, 0x47, 0x34, 0x01 } } 
        };

        /* MiKettle characteristics */
        static constexpr esp_bt_uuid_t GATT_UUID_AUTH_INIT      { ESP_UUID_LEN_16, { 0x0010 } };
        static constexpr esp_bt_uuid_t GATT_UUID_AUTH           { ESP_UUID_LEN_16, { 0x0001 } };
        static constexpr esp_bt_uuid_t GATT_UUID_VERSION        { ESP_UUID_LEN_16, { 0x0004 } };
        static constexpr esp_bt_uuid_t GATT_UUID_SETUP          { ESP_UUID_LEN_16, { 0xaa01 } };
        static constexpr esp_bt_uuid_t GATT_UUID_STATUS         { ESP_UUID_LEN_16, { 0xaa02 } };
        static constexpr esp_bt_uuid_t GATT_UUID_TIME           { ESP_UUID_LEN_16, { 0xaa04 } };
        static constexpr esp_bt_uuid_t GATT_UUID_BOIL_MODE      { ESP_UUID_LEN_16, { 0xaa05 } };
        static constexpr esp_bt_uuid_t GATT_UUID_MCU_VERSION    { ESP_UUID_LEN_16, { 0x2a28 } };
        static constexpr esp_bt_uuid_t GATT_UUID_CCCD           { ESP_UUID_LEN_16, { 0x2902 } };

        template<typename InputT, typename KeyT>
        static std::vector<uint8_t> cipher(const KeyT& key, const InputT& input)
        {
            std::vector<uint8_t> result;
            std::array<uint8_t, PERM_LENGTH> perm;

            uint16_t index1 = 0;
            uint16_t index2 = 0;
            uint16_t index3 = 0;

            std::iota(perm.begin(), perm.end(), 0);

            uint16_t j = 0;
            for (uint16_t i = 0; i < perm.size(); i++)
            {
                j += perm[i] + key[i % key.size()];
                j &= 0xff;
                std::swap(perm[i], perm[j]);
            }

            for (uint16_t i = 0; i < input.size(); i++)
            {
                ++index1;
                index1 &= 0xff;
                index2 += perm[index1];
                index2 &= 0xff;
                std::swap(perm[index1], perm[index2]);
                index3 = perm[index1] + perm[index2];
                index3 &= 0xff;
                result.push_back((input[i] ^ perm[index3]) & 0xff);
            }

            return result;
        }

        template<typename ContainerT>
        static std::vector<uint8_t> mix_a(const ContainerT& data, const uint16_t product_id)
        {
            std::vector<uint8_t> result(8);

            result[0] = data[0];
            result[1] = data[2];
            result[2] = data[5];
            result[3] = product_id & 0xff;
            result[4] = product_id & 0xff;
            result[5] = data[4];
            result[6] = data[5];
            result[7] = data[1];

            return result;
        }

        template<typename ContainerT>
        static std::vector<uint8_t> mix_b(const ContainerT& data, const uint16_t product_id)
        {
            std::vector<uint8_t> result(8);

            result[0] = data[0];
            result[1] = data[2];
            result[2] = data[5];
            result[3] = (product_id >> 8) & 0xff;
            result[4] = data[4];
            result[5] = data[0];
            result[6] = data[5];
            result[7] = product_id & 0xff;

            return result;
        }
    };
}

#endif