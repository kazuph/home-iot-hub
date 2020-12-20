#ifndef HUB_TEST_H
#define HUB_TEST_H

typedef enum test_err_t
{
    TEST_SUCCESS = 0,
    TEST_FAILURE = -1,
    TEST_SETUP_FAILURE = -2,
    TEST_TIMEOUT = -3,
    TEST_UNKNOWN_ID = -4
} test_err_t;

/*
*   Run tests. Function waits for serial input, which specifies ID of the
*   desired test.
*/
void test_run();

#endif