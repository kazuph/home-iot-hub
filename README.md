# Home Assistant BLE to MQTT bridge.

[![Actions Status](https://github.com/Chylynsky/home-iot-hub/workflows/Build/badge.svg)](https://github.com/Chylynsky/home-iot-hub/actions)

## About

This piece of software allows to integrate *BLE* devices with *Home Assistant* *MQTT Discovery* feature using
a single *ESP32* board.

## Requirements

* ESP32 board
* [Espressif IoT Development Framework](https://github.com/espressif/esp-idf)
* [Home Assistant](https://www.home-assistant.io/)
* [MQTT Home Assistant Integration](https://www.home-assistant.io/integrations/mqtt/)

## System Overview

Home Assistant BLE to MQTT bridge integrates BLE devices with Home Assistant [MQTT Discovery](https://www.home-assistant.io/docs/mqtt/discovery/) feature. It allows to omit the need
of setting up custom integrations on the Home Assistant side. The device integrations running on the bridge are the only ones responsible for the proper integration with MQTT Discovery. 

## Configuration JSON

Configuration is done via a JSON file, flashed to the ESP32 board along with the compiled program. ESP-IDF allows to flash them seperately, so there is no need for program re-flashing when only the configuration changes.
The configuration JSON is an **object** with *"general"*, *"wifi"*, *"mqtt"* and *"devices"* keys.

### *general*

This **object** contains the below field:

* *"discovery_prefix"* - the prefix for the discovery topic

### *wifi*

This **object** contains the below fields:

* *"ssid"* - your Wi-Fi SSID
* *"password"* - you Wi-FI password

### *mqtt*

This **object** contains the below field:

* *"uri"* - URI of the MQTT broker to be used. **Must** be the same as the one set-up for the Home Assistant MQTT integration.
...Example: "mqtt://123.456.7.890:1883"

### *devices*

A **list** of **objects**, where *each* object has the below fields:

* *"name"* - name of the BLE device
* *"mac"* - MAC address of the BLE device. Only *public* addresses are supported as of now.

### Example JSON configuration

```json
{
    "general": {
        "discovery_prefix": "homeassistant"
    },
    "wifi": {
        "ssid": "MyWifi",
        "password": "safepassword123"
    },
    "mqtt": {
        "uri": "mqtt://123.456.7.890:1883"
    },
    "devices": [
        {
            "name": "Bulb0",
            "mac": "00:00:00:00:00:00"
        },
        {
            "name": "Bulb1",
            "mac": "FF:FF:FF:FF:FF:FF"
        }
    ]
}
```

## Device Integrations

In progress...

## Adding a custom device integration

In progress...

## Dependencies

All the dependencies are downloaded during by the CMake during build.

* [rxcpp](https://github.com/ReactiveX/RxCpp)
* [tl::expected](https://github.com/TartanLlama/expected)
* [rapidjson](https://github.com/Tencent/rapidjson)
* [{fmt}](https://github.com/fmtlib/fmt)

## Author

* [Borys Chyliński](https://github.com/Chylynsky)
