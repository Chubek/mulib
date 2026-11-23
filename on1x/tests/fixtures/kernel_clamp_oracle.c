#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include "mulib-clamp.h"

int main(void) {
    const int64_t results[] = {
        mulib_clamp_add_i64(40, 2),
        mulib_clamp_add_i64(INT64_MAX, 1),
        mulib_clamp_sub_i64(INT64_MIN, 1),
        mulib_clamp_sub_i64(INT64_MAX, INT64_MIN),
        mulib_clamp_add_i64(INT64_MIN, -1),
        mulib_clamp_sub_i64(4, -2),
        mulib_clamp_i64(-1, 0, 10),
        mulib_clamp_i64(20, 0, 10),
        mulib_clamp_i64(4, 0, 10),
        mulib_clamp_i64(20, -7, -7),
    };
    for (size_t i = 0; i < sizeof results / sizeof results[0]; ++i)
        printf("%" PRId64 "\n", results[i]);
    return 0;
}
