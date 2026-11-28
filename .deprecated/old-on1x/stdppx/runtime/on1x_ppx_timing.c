/* C11 timing adapter; for Linux/POSIX prefers monotonic clock, otherwise C11 UTC. */
#define _POSIX_C_SOURCE 200809L
#include "on1x_ppx_timing.h"
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <limits.h>
#include <time.h>
int64_t on1x_ppx_clock_ns(void) {
    struct timespec t;
#if defined(CLOCK_MONOTONIC)
    if (clock_gettime(CLOCK_MONOTONIC, &t) != 0) {
        fputs("on1x PPX: monotonic clock failed\n", stderr); abort();
    }
#else
    if (timespec_get(&t, TIME_UTC) != TIME_UTC) {
        fputs("on1x PPX: C11 clock failed\n", stderr); abort();
    }
#endif
    if (t.tv_sec < 0 || t.tv_nsec < 0 || t.tv_nsec >= 1000000000L ||
        (uint64_t)t.tv_sec > ((uint64_t)INT64_MAX - (uint64_t)t.tv_nsec) / UINT64_C(1000000000)) {
        fputs("on1x PPX: nanosecond time out of range\n", stderr); abort();
    }
    return (int64_t)t.tv_sec * INT64_C(1000000000) + (int64_t)t.tv_nsec;
}
void on1x_ppx_timing_log(const char *name, int64_t duration) {
    if (!name) name = "<unnamed>";
    (void)fprintf(stderr, "on1x timing %s: %" PRId64 " ns\n", name, duration);
}
