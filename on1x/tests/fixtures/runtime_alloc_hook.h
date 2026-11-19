#ifndef ON1X_TEST_RUNTIME_ALLOC_HOOK_H
#define ON1X_TEST_RUNTIME_ALLOC_HOOK_H
#include <stddef.h>

extern size_t on1x_test_runtime_fail_after;
void *on1x_test_runtime_calloc(size_t, size_t);
void on1x_test_runtime_free(void *);

#endif
