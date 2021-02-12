#include "hub_const_map.h"
#include "hub_semaphore_lock.h"
#include "hub_dispatch_queue.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <array>
#include <algorithm>
#include <utility>

namespace hub
{
    extern utils::dispatch_queue<4096, tskIDLE_PRIORITY> task_queue;
}