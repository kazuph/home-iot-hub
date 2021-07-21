#include "app/cleanup.hpp"

#include "wifi/wifi.hpp"
#include "ble/ble.hpp"
#include "ble/scanner.hpp"
#include "filesystem/filesystem.hpp"

namespace hub
{
    void cleanup_t::cleanup() const noexcept
    {
        wifi::disconnect();
        ble::deinit();
        filesystem::deinit();
    }
}
