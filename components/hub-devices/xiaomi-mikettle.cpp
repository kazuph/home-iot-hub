#include "xiaomi-mikettle.hpp"

namespace hub::device::xiaomi
{
    void mikettle::on_after_connect(std::shared_ptr<ble::client> client)
    {
        auto kettle_service = client->get_service_by_uuid(&GATT_UUID_KETTLE_SRV);
        kettle_service.get_characteristic_by_uuid(&GATT_UUID_AUTH_INIT).write({ key1.cbegin(), key1.cend() });
        auto auth_characteristic = kettle_service.get_characteristic_by_uuid(&GATT_UUID_AUTH);

        client->set_notify_event_handler([](auto arg) {
            
        });

        auth_characteristic.subscribe();

        {
            auto auth_descriptors = auth_characteristic.get_descriptors();
            auth_descriptors[0].write({ subscribe.cbegin(), subscribe.cend() });
        }
    }

    utils::json mikettle::device_data_to_json(const ble::event::notify_event_args_t& device_data)
    {
        return utils::json();
    }

    void mikettle::send_data_to_device(utils::json&& data)
    {
        
    }
}