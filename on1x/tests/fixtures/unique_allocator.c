#include <stdlib.h>
#include <stdint.h>

static long live_allocations;
static long successful_allocations;
static long attempted_allocations;
static int verifier_registered;

static void verify_empty(void) {
    if (live_allocations != 0) abort();
}

void *on1x_test_unique_malloc(size_t size) {
    const char *limit = getenv("ON1X_TEST_UNIQUE_FAIL_AFTER");
    void *value;
    if (limit &&
        attempted_allocations++ >= strtol(limit, NULL, 10))
        return NULL;
    value = malloc(size);
    if (!value) return NULL;
    if (!verifier_registered) {
        if (atexit(verify_empty) != 0) abort();
        verifier_registered = 1;
    }
    ++live_allocations;
    ++successful_allocations;
    return value;
}

void on1x_test_unique_free(void *value) {
    if (!value || live_allocations <= 0) abort();
    --live_allocations;
    free(value);
}

int64_t on1x_test_unique_allocator_live(void) {
    return live_allocations;
}

int64_t on1x_test_unique_allocator_allocations(void) {
    return successful_allocations;
}
