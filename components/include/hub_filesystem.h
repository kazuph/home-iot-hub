#ifndef HUB_FILESYSTEM_H
#define HUB_FILESYSTEM_H

#include "esp_err.h"

#include <cstdio>
#include <string_view>

namespace hub::filesystem
{
    /**
     * @brief Initialize filesystem. Base path is set to "/spiffs". Max files is set to 5. The filesystem
     * is formatted if mount fails.
     * 
     * @return esp_err_t Initialization status.
     */
    esp_err_t init();

    /**
     * @brief Deinitialize filesystem.
     * 
     * @return esp_err_t Deinitialization status.
     */
    esp_err_t deinit();
}

#endif