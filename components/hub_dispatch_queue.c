#include "hub_dispatch_queue.h"

#include <string.h>

#include "esp_log.h"

#define TASK_NAME "DISPATCH QUEUE MAIN"

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

    if (xTaskCreate(&dispatch_fun, TASK_NAME, 2048, queue, tskIDLE_PRIORITY, &queue->task) != pdPASS)
    {
        ESP_LOGE(TAG, "Task create failed.");
        result = ESP_FAIL;
        goto cleanup_queue;
    }

    ESP_LOGI(TAG, "Dispatch queue created.");
    return result;

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
    vQueueDelete(queue->message_queue);
    queue->message_queue = NULL;

    return ESP_OK;
}

esp_err_t hub_dispatch_queue_push(hub_dispatch_queue* queue, dispatch_queue_fun_t fun)
{
    ESP_LOGD(TAG, "Function: %s.", __func__);
    esp_err_t result = push(queue, fun);

    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Push to message queue failed.");
        return result;
    }

    ESP_LOGI(TAG, "Push to dispatch queue success.");
    ESP_LOGI(TAG, "Current queue count: %i/%i.", (DISPATCH_QUEUE_SIZE - (int)uxQueueSpacesAvailable(queue->message_queue)), DISPATCH_QUEUE_SIZE);
    return result;
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
        return ESP_ERR_TIMEOUT;
    }

    return ESP_OK;
}