#ifndef ON1X_TEST_UNIQUE_ALLOCATOR_H
#define ON1X_TEST_UNIQUE_ALLOCATOR_H
#include <stddef.h>
#include <stdint.h>

void *on1x_test_unique_malloc(size_t size);
void on1x_test_unique_free(void *value);
int64_t on1x_test_unique_allocator_live(void);
int64_t on1x_test_unique_allocator_allocations(void);

#define ON1X_UNIQUE_MALLOC on1x_test_unique_malloc
#define ON1X_UNIQUE_FREE on1x_test_unique_free
#endif
