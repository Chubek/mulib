#ifndef ON1X_PPX_TIMING_H
#define ON1X_PPX_TIMING_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Exact C ABI for @timing("elapsed") generated raw On1x externs. */
int64_t on1x_ppx_clock_ns(void);
void on1x_ppx_timing_log(const char *function_name, int64_t elapsed_nanoseconds);
#ifdef __cplusplus
}
#endif
#endif
