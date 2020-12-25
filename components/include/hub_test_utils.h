#ifndef HUB_TEST_UTILS_H
#define HUB_TEST_UTILS_H

#include "esp_err.h"

typedef enum test_err_t
{
    TEST_SUCCESS = 0,
    TEST_FAILURE = -1,
    TEST_SETUP_FAILURE = -2,
    TEST_TIMEOUT = -3,
    TEST_UNKNOWN_ID = -4,
    TEST_NOT_RUN = -5
} test_err_t;

typedef struct test_case
{
    const char* name;
    test_err_t (*run)();
} test_case;

esp_err_t stdio_config();
esp_err_t wifi_config();

#endif