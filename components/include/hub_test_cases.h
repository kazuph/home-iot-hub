#ifndef HUB_TEST_CASES_H
#define HUB_TEST_CASES_H

#include "hub_test_utils.h"

#define N_TESTS 2

#define TEST_WIFI_CONNECT_DISCONNECT    0
#define TEST_MQTT_ECHO                  1

extern test_case* TEST_CASES[N_TESTS];

void export_tests();

#endif