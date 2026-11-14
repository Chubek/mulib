#ifndef ON1X_TEST_CLOSURE_ALLOCATOR_H
#define ON1X_TEST_CLOSURE_ALLOCATOR_H
#include <stddef.h>
void *on1x_test_env_malloc(size_t size);
void on1x_test_env_free(void *value);
#define ON1X_ENV_MALLOC on1x_test_env_malloc
#define ON1X_ENV_FREE on1x_test_env_free
#endif
