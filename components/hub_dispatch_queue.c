#include "hub_dispatch_queue.h"

#include <string.h>
#include <stdbool.h>

#include "esp_log.h"

#define TASK_NAME "DISPATCH QUEUE MAIN"
#define INIT_SIZE 4

#define EMPTY_BIT   BIT0
#define FULL_BIT    BIT1

static const char* TAG = "HUB_DISPATCH_QUEUE";

static void dispatch_fun(void* args);
static dispatch_queue_fun_t pop(hub_dispatch_queue* queue);
static void push(hub_dispatch_queue* queue, dispatch_queue_fun_t fun);

void hub_dispatch_queue_init(hub_dispatch_queue* queue)
{
    ESP_LOGD(TAG, "Function: %s.", __func__);

    BaseType_t result = pdPASS;

    queue->message_queue = xQueueCreate(DISPATCH_QUEUE_SIZE, (UBaseType_t)sizeof(dispatch_queue_fun_t));
    if (queue->message_queue == NULL)
    {
        ESP_LOGE(TAG, "Queue create failed.");
        return;
    }

    queue->event_group = xEventGroupCreate();
    if (queue->event_group == NULL)
    {
        ESP_LOGE(TAG, "Event group create failed.");
        return;
    }

    result = xTaskCreate(&dispatch_fun, TASK_NAME, 2048, queue, tskIDLE_PRIORITY, &queue->task);
    if (result != pdPASS)
    {
        ESP_LOGE(TAG, "Task create failed.");
        return;
    }

    ESP_LOGI(TAG, "Dispatch queue created.");
}

void hub_dispatch_queue_destroy(hub_dispatch_queue* queue)
{
    ESP_LOGD(TAG, "Function: %s.", __func__);

    vTaskDelete(queue->task);
    queue->task = NULL;
    vEventGroupDelete(queue->event_group);
    queue->event_group = NULL;
    vQueueDelete(queue->message_queue);
}

void hub_dispatch_queue_push(hub_dispatch_queue* queue, dispatch_queue_fun_t fun)
{
    ESP_LOGD(TAG, "Function: %s.", __func__);
    push(queue, fun);
}

void hub_dispatch_queue_push_n(hub_dispatch_queue* queue, dispatch_queue_fun_t* fun, uint16_t length)
{
    ESP_LOGD(TAG, "Function: %s.", __func__);
    for (uint16_t i = 0; i < length; i++)
    {
        push(queue, fun[i]);
        fun++;
    }
}

static void dispatch_fun(void* args)
{
    ESP_LOGD(TAG, "Function: %s.", __func__);

    hub_dispatch_queue* queue = (hub_dispatch_queue*)args;
    dispatch_queue_fun_t fun = NULL;

    while (1)
    {
        fun = NULL;
        fun = pop(queue);

        if (fun == NULL)
        {
            continue;
        }

        fun();
    }

    ESP_LOGW(TAG, "Dispatch function has ended.");
    vTaskDelete(NULL);
}

static dispatch_queue_fun_t pop(hub_dispatch_queue* queue)
{
    ESP_LOGD(TAG, "Function: %s.", __func__);

    dispatch_queue_fun_t fun = NULL;

    BaseType_t result = xQueueReceive(queue->message_queue, (void*)&fun, DISPATCH_QUEUE_POP_TIMEOUT);
    if (result != pdTRUE)
    {
        return NULL;
    }

    return fun;
}

static void push(hub_dispatch_queue* queue, dispatch_queue_fun_t fun)
{
    ESP_LOGD(TAG, "Function: %s.", __func__);

    BaseType_t result = xQueueSendToBack(queue->message_queue, (const void*)&fun, DISPATCH_QUEUE_PUSH_TIMEOUT);
    if (result != pdTRUE)
    {
        ESP_LOGE(TAG, "Message queue send failed.");
    }
}