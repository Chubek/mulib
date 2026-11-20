#include "mulib-media-timebase.h"
#include "mulib-img-core.h"

int mulib_joltfx_peer_check(void)
{
    int64_t tick = 0;
    return mulib_media_timebase_rescale(INT64_MIN, (mulib_media_rational){1, 1},
                                       (mulib_media_rational){1, 1}, &tick) &&
           tick == INT64_MIN;
}
