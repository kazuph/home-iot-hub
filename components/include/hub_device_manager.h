#ifndef HUB_DEVICE_MANAGER_H
#define HUB_DEVICE_MANAGER_H

#include "hub_mqtt.h"
#include "hub_device.h"
#include "hub_ble_mac.h"
#include "hub_json.h"

#include <memory>
#include <map>
#include <string_view>

namespace hub
{
    /*
        Class responsible for managing BLE devices, controlled via MQTT protocol.
    */
    class device_manager
    {
    public:

        /* MQTT topics by which the device_manager instance is controlled. */
        static constexpr std::string_view MQTT_BLE_SCAN_ENABLE_TOPIC        { "/scan/enable" };
        static constexpr std::string_view MQTT_BLE_SCAN_RESULTS_TOPIC       { "/scan/results" };
        static constexpr std::string_view MQTT_BLE_CONNECT_TOPIC            { "/connect" };
        static constexpr std::string_view MQTT_BLE_DISCONNECT_TOPIC         { "/disconnect" };
        static constexpr std::string_view MQTT_BLE_DEVICE_READ_TOPIC        { "/device/read" };
        static constexpr std::string_view MQTT_BLE_DEVICE_WRITE_TOPIC       { "/device/write" };

        static constexpr std::string_view CONNECTED_DEVICES_FILE_NAME       { "connected_devices.json" };

        static constexpr uint16_t BLE_DEFAULT_SCAN_TIME{ 30 }; // seconds

        device_manager();

        device_manager(device_manager&& other)              = default;

        device_manager(const device_manager&)               = delete;

        device_manager& operator=(device_manager&&)         = default;

        device_manager& operator=(const device_manager&)    = delete;

        esp_err_t mqtt_start(std::string_view mqtt_uri, const uint16_t mqtt_port);

        esp_err_t mqtt_stop();

        ~device_manager();

    private:

        static constexpr const char* TAG{ "device_manager" };

        mqtt::client                                                    mqtt_client;            /*  MQTT client used to manage devices. */

        std::map<ble::mac, std::string_view>                            scan_results;           /*  Cache scan results in order to avoid resending the same scan results over MQTT. 
                                                                                                    Scan results are cached only for the period of BLE scan. */

        std::map<std::string_view, std::unique_ptr<hub::device_base>>   connected_devices;      /*  Map holding devices connected at the given time. */

        std::map<std::string_view, std::unique_ptr<hub::device_base>>   disconnected_devices;   /*  Map holding devices that are currently disconnected. Reconnection trial is issued
                                                                                                    every time the device is found during BLE scan. */

        /*  Dumps devices stored in connected_devices and disconnected_devices maps in a form of a JSON file.
            The dumped file is used on application startup to perform device reconnection. */
        void dump_connected_devices();

        /*  Loads devices that were connected (and those disconnected) during the previous application run.
            Devices are loaded from the JSON file dumped by the dump_connected_devices method and stored in
            disconnected_devices map to ensure their reconnection whenever they are discovered by the BLE scan. */
        void load_connected_devices();

        /*  Start BLE scan for the given time. If no argument is given, the scan time is BLE_DEFAULT_SCAN_TIME. */
        void ble_scan_start(uint16_t ble_scan_time = BLE_DEFAULT_SCAN_TIME);

        /*  Stop BLE scan. */
        void ble_scan_stop();

        /*  Callback method called whenever new scan results are available. */
        void ble_scan_callback(std::string_view name, const ble::mac& address);

        /*  Connect to the specified device. Device must be supported (listed in device_factory::supported_devices const_map).
            After successful connection, the device is stored in connected_devices map. If connection fails, the device is stored in
            disconnected_devices map.
            Throws std::runtime_error upon failure or if device is not supported. */
        void ble_device_connect(std::string_view id, std::string_view name, const ble::mac& address);

        /*  Disconnect with the given device. If device is not in connected_devices or disconnected_devices maps, the method has no effect. */
        void ble_device_disconnect(std::string_view id) noexcept;

        /*  Main MQTT data callback. Calls topic-specific methods if the topic is supported. */
        void mqtt_data_callback(std::string_view topic, std::string_view data);

        /*  Callback called by the mqtt_data_event_callback if the topic is MQTT_BLE_SCAN_ENABLE_TOPIC. */
        void mqtt_scan_enable_topic_callback(const json::json& data);

        /*  Callback called by the mqtt_data_event_callback if the topic is MQTT_BLE_CONNECT_TOPIC. */
        void mqtt_connect_topic_callback(const json::json& data);

        /*  Callback called by the mqtt_data_event_callback if the topic is MQTT_BLE_DISCONNECT_TOPIC. */
        void mqtt_disconnect_topic_callback(const json::json& data);

        /*  Callback called by the mqtt_data_event_callback if the topic is MQTT_BLE_DEVICE_WRITE_TOPIC. */
        void mqtt_device_write_topic_callback(const json::json& data);
    };
}

#endif