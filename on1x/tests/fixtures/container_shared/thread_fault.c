#include "thread_fault.h"
#include <stdlib.h>

int on1x_test_shared_create(mulib_xthread_t *out, mulib_xthread_fn body, void *arg) {
    static long launches;
    const char *limit = getenv("ON1X_TEST_SHARED_THREAD_FAIL_AFTER");
    if (limit && launches++ >= strtol(limit, NULL, 10))
        return MULIB_XTHREAD_NOMEM;
    return mulib_xthread_create(out, body, arg);
}
