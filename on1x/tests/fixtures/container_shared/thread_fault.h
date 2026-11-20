#ifndef ON1X_TEST_SHARED_THREAD_FAULT_H
#define ON1X_TEST_SHARED_THREAD_FAULT_H
#include <mulib-xthreads.h>
int on1x_test_shared_create(mulib_xthread_t *, mulib_xthread_fn, void *);
#define ON1X_SHARED_THREAD_CREATE on1x_test_shared_create
#endif
