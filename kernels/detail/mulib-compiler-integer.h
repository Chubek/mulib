/* Internal, checked signed arithmetic shared by compiler modules. */
#ifndef MULIB_COMPILER_INTEGER_H
#define MULIB_COMPILER_INTEGER_H
#include <stdint.h>
static inline int mulib_compiler_add_i64_(int64_t a, int64_t b, int64_t *out)
{
    if ((b > 0 && a > INT64_MAX - b) || (b < 0 && a < INT64_MIN - b)) return 0;
    *out = a + b;
    return 1;
}
static inline int mulib_compiler_sub_i64_(int64_t a, int64_t b, int64_t *out)
{
    if ((b < 0 && a > INT64_MAX + b) || (b > 0 && a < INT64_MIN + b)) return 0;
    *out = a - b;
    return 1;
}
static inline int mulib_compiler_mul_i64_(int64_t a, int64_t b, int64_t *out)
{
    if (!a || !b) { *out = 0; return 1; }
    if ((a == INT64_MIN && b == -1) || (b == INT64_MIN && a == -1)) return 0;
    if (a > 0) {
        if ((b > 0 && a > INT64_MAX / b) || (b < 0 && b < INT64_MIN / a)) return 0;
    } else {
        if ((b > 0 && a < INT64_MIN / b) || (b < 0 && a < INT64_MAX / b)) return 0;
    }
    *out = a * b;
    return 1;
}
#endif
