#include "hub_test_wifi.h"

#include "hub_wifi.h"

static const char* TAG = "HUB_WIFI_TEST";

static test_err_t run()
{
    test_err_t test_result = TEST_SUCCESS;
    esp_err_t result = wifi_config();

    if (result != ESP_OK)
    {
        test_result = TEST_FAILURE;
        return test_result;
    }

    hub_wifi_disconnect();
    return test_result;
}

test_case test_wifi_connect_disconnect = {
    .name = "TEST WIFI CONNECT DISCONNECT",
    .run = &run
};