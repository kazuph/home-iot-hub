#ifndef HUB_DISPATCH_QUEUE_H
#define HUB_DISPATCH_QUEUE_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"

#ifndef DISPATCH_QUEUE_SIZE
#define DISPATCH_QUEUE_SIZE                 (UBaseType_t)16
#endif

#ifndef DISPATCH_QUEUE_TIMEOUT_DEFAULT
#define DISPATCH_QUEUE_TIMEOUT_DEFAULT      (TickType_t)10000 / portTICK_PERIOD_MS
#endif

#ifndef DISPATCH_QUEUE_PUSH_TIMEOUT
#define DISPATCH_QUEUE_PUSH_TIMEOUT         DISPATCH_QUEUE_TIMEOUT_DEFAULT
#endif

#ifndef DISPATCH_QUEUE_POP_TIMEOUT
#define DISPATCH_QUEUE_POP_TIMEOUT          DISPATCH_QUEUE_TIMEOUT_DEFAULT
#endif

typedef void (*dispatch_queue_fun_t)();

typedef struct hub_dispatch_queue
{
    TaskHandle_t task;
    EventGroupHandle_t event_group;
    QueueHandle_t message_queue;
} hub_dispatch_queue;

void hub_dispatch_queue_init(hub_dispatch_queue* queue);

void hub_dispatch_queue_destroy(hub_dispatch_queue* queue);

void hub_dispatch_queue_push(hub_dispatch_queue* queue, dispatch_queue_fun_t fun);

void hub_dispatch_queue_push_n(hub_dispatch_queue* queue, dispatch_queue_fun_t* fun, uint16_t length);

#endif