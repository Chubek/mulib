/* Counts only closure environments, not JSON or compiler allocations. */
#include "on1x_runtime.h"
#include <stdlib.h>

static int64_t live, peak, allocations;
static void verify_empty(void) {
    /* Fatal allocation errors deliberately terminate without unwinding. */
    if (live && !getenv("ON1X_TEST_ENV_FAIL_AFTER")) abort();
}

void *on1x_test_env_malloc(size_t size) {
    const char *limit = getenv("ON1X_TEST_ENV_FAIL_AFTER");
    void *value;
    if (limit && (unsigned long long)allocations >= strtoull(limit, NULL, 10))
        return NULL;
    value = malloc(size);
    if (value) {
        if (!allocations && atexit(verify_empty) != 0) abort();
        ++allocations;
        if (++live > peak) peak = live;
    }
    return value;
}

void on1x_test_env_free(void *value) {
    if (!value || live <= 0) abort();
    --live;
    free(value);
}

int64_t live_environments(void) { return live; }
int64_t peak_environments(void) { return peak; }
int64_t environment_allocations(void) { return allocations; }
