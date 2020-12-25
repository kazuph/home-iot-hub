#include "hub_test_cases.h"
#include "hub_test_utils.h"

#include "hub_test_wifi.h"
#include "hub_test_mqtt.h"

test_case* TEST_CASES[N_TESTS];

void export_tests()
{
    TEST_CASES[TEST_WIFI_CONNECT_DISCONNECT] = &test_wifi_connect_disconnect;
    TEST_CASES[TEST_MQTT_ECHO] = &test_mqtt_echo;
}