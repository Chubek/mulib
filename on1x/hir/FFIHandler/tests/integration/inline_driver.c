/* Host reference for the real header-only unsigned function. */
#include <inttypes.h>
#include <stdio.h>

#include <mulib-container-core.h>
#include <mulib-media-timebase.h>

_Static_assert(_Generic(&mulib_media_timebase_gcd, uint64_t (*)(uint64_t, uint64_t): 1,
                        default: 0), "timebase native prototype");
_Static_assert(_Generic(&mulib_ct_counter_add, uint64_t (*)(uint64_t, uint64_t): 1,
                        default: 0), "counter-add native prototype");

int main(void) {
    uint64_t a, b;
    int scanned;
    while ((scanned = scanf("%" SCNu64 " %" SCNu64, &a, &b)) == 2)
        printf("%" PRIu64 " %" PRIu64 "\n",
               mulib_media_timebase_gcd(a, b), mulib_ct_counter_add(a, b));
    return scanned == EOF && !ferror(stdin) && !ferror(stdout) ? 0 : 3;
}
