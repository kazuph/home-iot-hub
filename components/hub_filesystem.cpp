#include "hub_filesystem.h"

#include "esp_log.h"
#include "esp_spiffs.h"

#include <exception>
#include <stdexcept>

namespace hub::filesystem
{
    static constexpr const char* TAG{ "filesystem" };

    static constexpr esp_vfs_spiffs_conf_t fs_config{
        "/spiffs",  // base path
        nullptr,    // partition label
        5,          // max_files
        true        // format if mount failed
    };

    esp_err_t init()
    {
        return esp_vfs_spiffs_register(&fs_config);
    }

    esp_err_t deinit()
    {
        return esp_vfs_spiffs_unregister(fs_config.partition_label);
    }
}