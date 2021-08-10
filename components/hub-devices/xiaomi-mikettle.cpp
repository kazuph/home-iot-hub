#include "xiaomi-mikettle.hpp"

#include "timing/timing.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include <type_traits>

#include "esp_log.h"

namespace hub::device::xiaomi
{
    void mikettle::connect(utils::mac address)
    {
        using namespace timing::literals;

        static constexpr EventBits_t AUTH_BIT{ BIT0 };

        auto client = get_client();

        client->connect(address);

        auto kettle_service = client->get_service_by_uuid(&GATT_UUID_KETTLE_SRV).value();

        auto auth_characteristic = kettle_service.get_characteristic_by_uuid(&GATT_UUID_AUTH).value();

        kettle_service
            .get_characteristic_by_uuid(&GATT_UUID_AUTH_INIT)
            .value()
            .write({ key1.cbegin(), key1.cend() });
        
        auth_characteristic
            .get_descriptor_by_uuid(&GATT_UUID_CCCD)
            .value()
            .write({ subscribe.cbegin(), subscribe.cend() });

        {
            static_assert(std::is_pointer_v<EventGroupHandle_t>, "EventGroupHandle_t is not a pointer.");
            static_assert(std::is_same_v<EventGroupHandle_t, void*>, "EventGroupHandle_t is not a void pointer.");
            std::shared_ptr<void> auth_event_group{ xEventGroupCreate(), &vEventGroupDelete };

            auth_characteristic.subscribe([auth_event_group](const std::vector<uint8_t>& data) {
                xEventGroupSetBits(auth_event_group.get(), AUTH_BIT);
            });

            {
                std::array<uint8_t, 6> reversed_mac;
                std::reverse_copy(
                    static_cast<const uint8_t*>(address), 
                    static_cast<const uint8_t*>(address) + utils::mac::MAC_SIZE, 
                    reversed_mac.begin());

                auth_characteristic.write(cipher(mix_a(reversed_mac, PRODUCT_ID), token));
            }

            EventBits_t bits = xEventGroupWaitBits(auth_event_group.get(), AUTH_BIT, pdTRUE, pdFALSE, static_cast<TickType_t>(5_s));

            if (!(bits & AUTH_BIT))
            {
                throw std::runtime_error("Could not authenticate.");
            }
        }

        auth_characteristic.write(cipher(token, key2));

        kettle_service
            .get_characteristic_by_uuid(&GATT_UUID_VERSION)
            .value()
            .read();

        auth_characteristic.unsubscribe();

        {
            auto kettle_data_service    = client->get_service_by_uuid(&GATT_UUID_KETTLE_DATA_SRV).value();
            auto status_characteristic  = kettle_data_service.get_characteristic_by_uuid(&GATT_UUID_STATUS).value();

            status_characteristic
                .get_descriptor_by_uuid(&GATT_UUID_CCCD)
                .value()
                .write({ subscribe.cbegin(), subscribe.cend() });

            status_characteristic.subscribe([this](const std::vector<uint8_t>& data) {
                rapidjson::Document result;
                
                result.SetObject();
                result.AddMember("action",  rapidjson::Value(data[0]), result.GetAllocator());
                result.AddMember("mode",    rapidjson::Value(data[1]), result.GetAllocator());

                {
                    auto temperature = rapidjson::Value(rapidjson::kObjectType);

                    temperature.AddMember("set",        rapidjson::Value(data[4]), result.GetAllocator());
                    temperature.AddMember("current",    rapidjson::Value(data[5]), result.GetAllocator());

                    result.AddMember("temperature", temperature, result.GetAllocator());
                }

                {
                    auto keep_warm = rapidjson::Value(rapidjson::kObjectType);

                    keep_warm.AddMember("type", rapidjson::Value(data[6]),                  result.GetAllocator());
                    keep_warm.AddMember("time", rapidjson::Value((data[7] << 8) & data[8]), result.GetAllocator());

                    result.AddMember("keep_warm", keep_warm, result.GetAllocator());
                }

                invoke_message_handler(std::move(result));
            });
        }
    }

    void mikettle::disconnect()
    {
        auto client = get_client();
        client->disconnect();

        return;
    }

    void mikettle::process_message(in_message_t&& message)
    {
        return;
    }
}