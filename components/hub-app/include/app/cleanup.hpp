#ifndef HUB_APP_CLEANUP_HPP
#define HUB_APP_CLEANUP_HPP

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

        void cleanup() const noexcept;
    };
}

#endif