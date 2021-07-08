#ifndef HUB_APP_CLEANUP_HPP
#define HUB_APP_CLEANUP_HPP

#include "wifi/wifi.hpp"
#include "ble/ble.hpp"
#include "ble/scanner.hpp"
#include "filesystem/filesystem.hpp"

namespace hub
{
    struct cleanup_t
    {
    public:

        cleanup_t()                                 = default;

        cleanup_t(const cleanup_t&)                 = delete;

        cleanup_t(cleanup_t&&)                      = default;

        cleanup_t& operator=(const cleanup_t&)      = delete;

        cleanup_t& operator=(cleanup_t&&)           = default;

        ~cleanup_t()                                = default;

        void cleanup() const noexcept
        {
            wifi::disconnect();
            ble::scanner::deinit();
            ble::deinit();
            filesystem::deinit();
        }
    };
}

#endif