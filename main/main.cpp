#include "app/application.hpp"

namespace hub
{
    extern "C" void app_main()
    {
        application app;
        app.run();
    }
}