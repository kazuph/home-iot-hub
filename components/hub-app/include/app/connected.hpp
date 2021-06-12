#ifndef HUB_CONNECTED_HPP
#define HUB_CONNECTED_HPP

#include "configuration.hpp"

namespace hub
{
    class connected_t
    {
    public:

        connected_t()                               = delete;

        connected_t(const configuration& config) :
            m_config{ std::cref(config) }
        {

        }

        connected_t(const connected_t&)             = delete;

        connected_t(connected_t&&)                  = default;

        connected_t& operator=(const connected_t&)  = delete;

        connected_t& operator=(connected_t&&)       = default;

        ~connected_t()                              = default;

        void process_events() const
        {
            return;
        }

    private:

        std::reference_wrapper<const configuration> m_config;
    };
}

#endif