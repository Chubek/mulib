/* A test-only C caller of the real header, not an On1x-generated adapter. */
#include <mulib-clamp.h>

#if defined(QUALIFY_WRONG_PRIMITIVE)
typedef long long probe_int;
_Static_assert(sizeof(probe_int) == sizeof(int64_t), "not a same-width control");
#else
typedef int64_t probe_int;
#endif

_Static_assert(_Generic(&mulib_clamp_add_i64, probe_int (*)(probe_int, probe_int): 1,
                        default: 0), "clamp native prototype");
_Static_assert(_Generic(&mulib_clamp_sub_i64, int64_t (*)(int64_t, int64_t): 1,
                        default: 0), "clamp native prototype");
_Static_assert(_Generic(&mulib_clamp_i64, int64_t (*)(int64_t, int64_t, int64_t): 1,
                        default: 0), "clamp native prototype");

int64_t probe_add(int64_t a, int64_t b) { return mulib_clamp_add_i64(a, b); }
int64_t probe_sub(int64_t a, int64_t b) { return mulib_clamp_sub_i64(a, b); }
int64_t probe_interval(int64_t value, int64_t lo, int64_t hi) {
    return mulib_clamp_i64(value, lo, hi);
}
