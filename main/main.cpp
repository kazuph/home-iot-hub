#include "application.h"

extern "C" void app_main()
{
    hub::application app{};
    app.run();
}