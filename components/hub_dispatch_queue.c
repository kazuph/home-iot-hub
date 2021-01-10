#include "hub_dispatch_queue.h"

#include <string.h>

#include "esp_log.h"

#define TASK_NAME "DISPATCH QUEUE MAIN"
#define INIT_SIZE 4

#define EMPTY_BIT   BIT0
#define FULL_BIT    BIT1

static const char* TAG = "HUB_DISPATCH_QUEUE";

static void dispatch_fun(void* args);
static dispatch_queue_fun_t pop(hub_dispatch_queue* queue);
static esp_err_t push(hub_dispatch_queue* queue, dispatch_queue_fun_t fun);

esp_err_t hub_dispatch_queue_init(hub_dispatch_queue* queue)
{
    ESP_LOGD(TAG, "Function: %s.", __func__);

    esp_err_t result = ESP_OK;

    queue->message_queue = xQueueCreate(DISPATCH_QUEUE_SIZE, (UBaseType_t)sizeof(dispatch_queue_fun_t));
    if (queue->message_queue == NULL)
    {
        ESP_LOGE(TAG, "Queue create failed.");
        return ESP_FAIL;
    }

    queue->event_group = xEventGroupCreate();
    if (queue->event_group == NULL)
    {
        ESP_LOGE(TAG, "Event group create failed.");
        result = ESP_FAIL;
        goto cleanup_queue;
    }

    if (xTaskCreate(&dispatch_fun, TASK_NAME, 2048, queue, tskIDLE_PRIORITY, &queue->task) != pdPASS)
    {
        ESP_LOGE(TAG, "Task create failed.");
        result = ESP_FAIL;
        goto cleanup_task;
    }

    ESP_LOGI(TAG, "Dispatch queue created.");
    return result;

cleanup_task:
    vTaskDelete(queue->task);
    queue->task = NULL;
cleanup_queue:
    vQueueDelete(queue->message_queue);
    queue->message_queue = NULL;

    return result;
}

esp_err_t hub_dispatch_queue_destroy(hub_dispatch_queue* queue)
{
    ESP_LOGD(TAG, "Function: %s.", __func__);

    vTaskDelete(queue->task);
    queue->task = NULL;
    vEventGroupDelete(queue->event_group);
    queue->event_group = NULL;
    vQueueDelete(queue->message_queue);
    queue->message_queue = NULL;

    return ESP_OK;
}

esp_err_t hub_dispatch_queue_push(hub_dispatch_queue* queue, dispatch_queue_fun_t fun)
{
    ESP_LOGD(TAG, "Function: %s.", __func__);
    return push(queue, fun);
}

esp_err_t hub_dispatch_queue_push_n(hub_dispatch_queue* queue, dispatch_queue_fun_t* fun, uint16_t length)
{
    ESP_LOGD(TAG, "Function: %s.", __func__);

    esp_err_t result = ESP_OK;

    for (uint16_t i = 0; i < length || result != ESP_OK; i++)
    {
        result = push(queue, fun[i]);
        fun++;
    }

    return result;
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

static esp_err_t push(hub_dispatch_queue* queue, dispatch_queue_fun_t fun)
{
    ESP_LOGD(TAG, "Function: %s.", __func__);

    BaseType_t result = xQueueSendToBack(queue->message_queue, (const void*)&fun, DISPATCH_QUEUE_PUSH_TIMEOUT);
    if (result != pdTRUE)
    {
        ESP_LOGE(TAG, "Message queue send failed.");
        return ESP_FAIL;
    }

    return ESP_OK;
}