#include "hub_test.h"

#include "hub_test_cases.h"
#include "hub_test_utils.h"

#include "hub_wifi.h"
#include "hub_mqtt.h"

#include <stdio.h>

#include "esp_event.h"
#include "esp_log.h"

#include "nvs.h"
#include "nvs_flash.h"

static const char* TAG = "HUB_MAIN_TEST";

void test_run()
{
    int test_id = -1;

    stdio_config();

    if (nvs_flash_init() != ESP_OK)
    {
        ESP_LOGE(TAG, "NVS flash initialization failed.");
        esp_restart();
    }

    if (esp_event_loop_create_default() != ESP_OK)
    {
        ESP_LOGE(TAG, "Default event loop creation failed.");
        esp_restart();
    }

    export_tests();

    while (1)
    {
        test_id = -1;
        while (test_id == -1)
        {
            vTaskDelay(20 / portTICK_PERIOD_MS);
            test_id = fgetc(stdin);
        }

        if (test_id >= 0 && test_id < N_TESTS)
        {
            printf("%s\n", TEST_CASES[test_id]->name);
            printf("Test ID %i.\n", test_id);
            test_err_t result = TEST_CASES[test_id]->run();
            printf("TEST RESULT: %i.\n", (int)result);
        }
        else
        {
            printf("Unknown test ID %i.\n", test_id);
        }
    }
}