#include "xiaomi-mikettle.hpp"

#include "timing/timing.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include <type_traits>

#include "esp_log.h"

namespace hub::device::xiaomi
{
    void mikettle::connect(ble::mac address)
    {
        using namespace timing::literals;

        static constexpr EventBits_t AUTH_BIT{ BIT0 };

        auto client = get_client();

        client->connect(address);

        auto kettle_service         = client->get_service_by_uuid(&GATT_UUID_KETTLE_SRV).get();

        auto auth_characteristic = kettle_service.get_characteristic_by_uuid(&GATT_UUID_AUTH);

        kettle_service.get_characteristic_by_uuid(&GATT_UUID_AUTH_INIT)->write({ key1.cbegin(), key1.cend() });
        auth_characteristic->get_descriptor_by_uuid(&GATT_UUID_CCCD)->write({ subscribe.cbegin(), subscribe.cend() });

        {
            static_assert(std::is_pointer_v<EventGroupHandle_t>, "EventGroupHandle_t is not a pointer.");
            static_assert(std::is_same_v<EventGroupHandle_t, void*>, "EventGroupHandle_t is not a void pointer.");
            std::shared_ptr<void> auth_event_group{ xEventGroupCreate(), &vEventGroupDelete };

            auth_characteristic->subscribe([auth_event_group](const ble::event::notify_event_args_t& data) {
                xEventGroupSetBits(auth_event_group.get(), AUTH_BIT);
            });

            {
                std::array<uint8_t, 6> reversed_mac;
                std::reverse_copy(address.cbegin(), address.cend(), reversed_mac.begin());

                auth_characteristic->write(cipher(mix_a(reversed_mac, PRODUCT_ID), token));
            }

            EventBits_t bits = xEventGroupWaitBits(auth_event_group.get(), AUTH_BIT, pdTRUE, pdFALSE, static_cast<TickType_t>(5_s));

            if (!(bits & AUTH_BIT))
            {
                throw std::runtime_error("Could not authenticate.");
            }
        }

        auth_characteristic->write(cipher(token, key2));

        kettle_service.get_characteristic_by_uuid(&GATT_UUID_VERSION)->read();

        auth_characteristic->unsubscribe();

        {
            auto kettle_data_service    = client->get_service_by_uuid(&GATT_UUID_KETTLE_DATA_SRV).get();
            auto status_characteristic  = kettle_data_service.get_characteristic_by_uuid(&GATT_UUID_STATUS);

            status_characteristic->get_descriptor_by_uuid(&GATT_UUID_CCCD)->write({ subscribe.cbegin(), subscribe.cend() });
            status_characteristic->subscribe([this](const ble::event::notify_event_args_t& data) {
                invoke_message_handler(rapidjson::Document());
            });
        }
    }

    void mikettle::disconnect()
    {
        return;
    }

    void mikettle::process_message(in_message_t&& message)
    {
        return;
    }
}