#ifndef ON1X_CONTAINER_SHARED_H
#define ON1X_CONTAINER_SHARED_H

#include "on1x_runtime.h"

/* Private direct-C11 ABI. One on1x_unique box represents one strong
 * reference. It is lexical and never crosses into a native worker. */
#define ON1X_CONTAINER_SHARED_ABI_VERSION 1
#define ON1X_SHARED_ARC_MAX_WORKERS 16

#ifdef __cplusplus
extern "C" {
#endif

on1x_unique *on1x_shared_v1_new(void *source, size_t size,
                               on1x_unique_destructor destroy, bool atomic);
on1x_unique *on1x_shared_v1_clone(on1x_unique *owner, bool atomic);
void on1x_shared_v1_read(on1x_unique *owner, bool atomic, void *out, size_t size);
int64_t on1x_shared_v1_parallel_count(on1x_unique *owner, size_t length,
                                      uint8_t byte, int64_t workers);
on1x_unique *on1x_shared_v1_cell_new(void *source, size_t size,
                                    on1x_unique_destructor destroy);
/* 1: granted, 0: busy, -1: allocation failure. Output is written only on grant. */
int on1x_shared_v1_cell_try_borrow(on1x_unique *owner, bool write, on1x_unique **out);
void on1x_shared_v1_cell_replace(on1x_unique *writer, void *source, void *out,
                                 size_t size);

#ifdef __cplusplus
}
#endif
#endif
