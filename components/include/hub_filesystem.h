#ifndef HUB_FILESYSTEM_H
#define HUB_FILESYSTEM_H

#include "esp_err.h"

#include <cstdio>
#include <string_view>

namespace hub::filesystem
{
    esp_err_t init();

    esp_err_t deinit();
}

#endif