/* Explicit wrapper exports, not implicit static-inline foreign linkage. */
#include "mulib-media-timebase.h"
#include "mulib-img-core.h"

int mulib_media_peer_check(void);

int main(void)
{
    int64_t tick = 0;
    return !(mulib_media_timebase_rescale(1, (mulib_media_rational){1, 24},
                                         (mulib_media_rational){1, 48000}, &tick) &&
             tick == 2000 && mulib_media_peer_check());
}
