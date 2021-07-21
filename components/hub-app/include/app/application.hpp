#ifndef HUB_APPLICATION_HPP
#define HUB_APPLICATION_HPP

#include "utils/fsm.hpp"

#include "cleanup.hpp"
#include "connected.hpp"
#include "init.hpp"
#include "not_connected.hpp"

namespace hub
{
    class application final : public utils::fsm<init_t, not_connected_t, connected_t, cleanup_t> 
    {
    public:
        application()                                   = default;

        application(const application &)                = delete;

        application(application &&)                     = default;

        application &operator=(const application &)     = delete;

        application &operator=(application &&)          = default;

        ~application()                                  = default;

        void run();

    private:

        static constexpr const char *TAG{ "hub::application" };
    };
}

#endif