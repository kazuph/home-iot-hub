#include "hub_dispatch_queue.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <array>
#include <algorithm>
#include <utility>

namespace hub
{
    extern dispatch_queue<4096, tskIDLE_PRIORITY> task_queue;
}