#ifndef ON1X_STDLIB_CONTAINER_RUNTIME_H
#define ON1X_STDLIB_CONTAINER_RUNTIME_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Native bridge ABI, not independently admitted KernelBind symbols. */
bool on1x_container_tls_i64_present(void);
int64_t on1x_container_tls_i64_value(void);
bool on1x_container_tls_i64_set(int64_t value);
void on1x_container_tls_i64_clear(void);

/* 0..255 byte; 256 EOF; -1 input, -2 open, -3 type, -4 stat/read, -5 close. */
int64_t on1x_container_file_byte_at(const char *path, int64_t offset);

#ifdef __cplusplus
}
#endif

#endif
