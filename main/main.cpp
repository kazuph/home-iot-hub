#include "app/application.hpp"

extern "C" void app_main()
{
    hub::application app;
    app.run();
}
