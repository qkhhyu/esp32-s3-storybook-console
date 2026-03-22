#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "rom/ets_sys.h"
#include <time.h>

#include "boost/config.hpp"

/**
 * @brief Temporary version to solve the problem of `nanosleep()` not being found
 * @param req: Time to sleep (seconds + nanoseconds)
 * @param rem: Remaining unslept time (can be NULL)
 * @return 0: Success, -1: Failure
 */
__attribute__((weak))
int nanosleep(const struct timespec *req, struct timespec *rem) {
    if (!req || req->tv_sec < 0 || req->tv_nsec < 0 || req->tv_nsec >= 1000000000) {
        return -1; // Invalid parameters
    }

    int64_t total_us = req->tv_sec * 1000000LL + req->tv_nsec / 1000;  // Convert to microseconds

    usleep((total_us + 1000 - 1) / 1000);

    return 0;  // Success
}

/**
 * @brief Temporary version to solve the problem of `getpagesize()` not being found
 *
 * thread: used when `BOOST_THREAD_USES_GETPAGESIZE` is defined
 *
 * @return 1: Success, -1: Failure
 */
__attribute__((weak))
int getpagesize() {
    return 1;
}
