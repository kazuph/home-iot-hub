#include "filesystem.hpp"

#include "esp_spiffs.h"

#include <exception>
#include <stdexcept>

namespace hub::filesystem
{
    static constexpr esp_vfs_spiffs_conf_t fs_config{
        "/spiffs",  // base path
        nullptr,    // partition label
        2,          // max_files
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