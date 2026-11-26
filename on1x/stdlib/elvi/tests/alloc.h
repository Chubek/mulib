#ifndef ON1X_STDLIB_ELVI_TEST_ALLOC_H
#define ON1X_STDLIB_ELVI_TEST_ALLOC_H
#include <stddef.h>
void *elvi_test_hit_alloc(size_t n);
void elvi_test_hit_free(void *p);
void *elvi_test_text_alloc(size_t n);
#endif
