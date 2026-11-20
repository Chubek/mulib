/*
 * Synthetic CPU image pipeline: no video decoder, GUI, GPU or On1x binding.
 * cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude \
 *    examples/media-pipeline.c -lm -o /tmp/opencode/media-pipeline
 */
#include "mulib-media-timebase.h"
#include "mulib-img-blur.h"
#include "mulib-color-grade.h"
#include "mulib-audio-mixer.h"
#include <stdio.h>

int main(void)
{
    /* Four RGBA float pixels, linear light and opaque straight alpha.
     * Stride/capacity are float counts, not byte counts. Arrays are disjoint. */
    float pixels[16] = {1, 0, 0, 1, 0, 1, 0, 1,
                        0, 0, 1, 1, 1, 1, 1, 1};
    float scratch[16], blurred[16], graded[16];
    mulib_img_view source = {pixels, 2, 2, 8, 16};
    mulib_img_view temporary = {scratch, 2, 2, 8, 16};
    mulib_img_view destination = {blurred, 2, 2, 8, 16};
    mulib_img_view output = {graded, 2, 2, 8, 16};
    mulib_color_grade_cdl grade = {{1.1f, 1.0f, 0.9f},
                                    {0, 0, 0}, {1, 1, 1}, 1};
    int64_t audio_tick = 0;
    float mono[2] = {0.25f, -0.25f}, stereo[4];
    mulib_audio_mixer_track track = {mono, 2, 1, 1, 0, false};
    if (!mulib_media_timebase_rescale(30, (mulib_media_rational){1, 30},
                                     (mulib_media_rational){1, 48000}, &audio_tick) ||
        !mulib_img_blur_gaussian(destination, temporary, source, 1, 1.0) ||
        !mulib_color_grade_apply(output, destination, grade) ||
        !mulib_audio_mixer(stereo, 2, &track, 1)) {
        fputs("CPU pipeline failed\n", stderr);
        return 1;
    }
    printf("30 video ticks = %lld audio ticks; graded first pixel %.6f %.6f %.6f; "
           "first stereo sample %.6f %.6f\n", (long long)audio_tick,
           (double)graded[0], (double)graded[1], (double)graded[2],
           (double)stereo[0], (double)stereo[1]);
    return 0;
}
