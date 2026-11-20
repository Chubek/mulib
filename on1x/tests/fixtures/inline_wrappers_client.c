#include "on1x-inline.h"

int main(void)
{
    const mulib_media_rational video = {1001, 30000};
    const mulib_media_rational audio = {1, 48000};
    const mulib_media_rational unit = {1, 1};
    const mulib_media_rational invalid = {1, 0};
    int64_t out = 0;
    if (!on1x_inline_v1_mulib_media_timebase_rescale(30000, video, audio, &out) ||
        out != 48048000)
        return 1;
    if (!on1x_inline_v1_mulib_media_timebase_rescale(-1, video, audio, &out) ||
        out != -1601)
        return 2;
    if (!on1x_inline_v1_mulib_media_timebase_rescale(INT64_MIN, unit, unit, &out) ||
        out != INT64_MIN)
        return 3;
    if (on1x_inline_v1_mulib_media_timebase_rescale(INT64_MAX, video, audio, &out) ||
        out != INT64_MIN ||
        on1x_inline_v1_mulib_media_timebase_rescale(5, invalid, unit, &out) ||
        out != INT64_MIN ||
        on1x_inline_v1_mulib_media_timebase_rescale(5, unit, unit, NULL))
        return 4;
    if (on1x_inline_v1_mulib_media_timebase_gcd(54, 24) != 6 ||
        !on1x_inline_v1_mulib_media_i64_add(9, -12, &out) ||
        out != -3 ||
        on1x_inline_v1_mulib_media_i64_add(INT64_MAX, 1, &out) ||
        out != -3)
        return 5;
    return 0;
}
