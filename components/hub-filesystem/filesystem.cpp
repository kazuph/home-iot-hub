#include "filesystem/filesystem.hpp"

#include "esp_spiffs.h"

namespace hub::filesystem
{
    static constexpr esp_vfs_spiffs_conf_t fs_config{
        "/spiffs",  // base path
        "storage",  // partition label
        2,          // max_files
        true        // format if mount failed
    };

    tl::expected<void, esp_err_t> init()
    {
        using result_type = tl::expected<void, esp_err_t>;

        if (esp_err_t result = esp_vfs_spiffs_register(&fs_config); result != ESP_OK)
        {
            return result_type(tl::unexpect, result);
        }

        return result_type();
    }

    tl::expected<void, esp_err_t> deinit()
    {
        using result_type = tl::expected<void, esp_err_t>;

        if (esp_err_t result = esp_vfs_spiffs_unregister(fs_config.partition_label); result != ESP_OK)
        {
            return result_type(tl::unexpect, result);
        }

        return result_type();
    }
}