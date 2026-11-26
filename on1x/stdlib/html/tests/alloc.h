#ifndef ON1X_HTML_TEST_ALLOC_H
#define ON1X_HTML_TEST_ALLOC_H
#include <stddef.h>
void *on1x_html_test_malloc(size_t n);
void *on1x_html_test_calloc(size_t count, size_t size);
void *on1x_html_test_realloc(void *p, size_t n);
void on1x_html_test_free(void *p);
void *on1x_html_test_fail_data_malloc(size_t n);
#endif
