/* Media integration. The source frames are synthetic sRGB RGBA8,
 * not the output of a MuLIB codec. The sink is packed sRGB RGB24. */
#include "mulib-color-grade.h"
#include "mulib-color-transfer.h"
#include "mulib-img-blend.h"
#include "mulib-media-frame.h"
#include "mulib-media-timebase.h"
#include "mulib-nle-edit.h"
#include "mulib-nle-evaluate.h"
#include "mulib-nle-ripple.h"
#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

typedef struct selection {
    unsigned count;
    uint64_t id;
    int64_t source;
} selection;

static bool
select_frame(void *state, const mulib_nle_item *item, int64_t source)
{
    selection *s = state;
    ++s->count;
    s->id = item->id;
    s->source = source;
    return s->count == 1;
}

static void
render(const mulib_media_frame *frame, uint8_t rgb[12])
{
    assert(mulib_media_frame_valid(frame) && frame->format == MULIB_MEDIA_RGBA8);
    assert(frame->width == 2 && frame->height == 2);
    float foreground[16], background[16], out[16];
    mulib_img_view fg = {foreground, 2, 2, 8, 16};
    mulib_img_view bg = {background, 2, 2, 8, 16};
    mulib_img_view dst = {out, 2, 2, 8, 16};
    for (size_t y = 0; y < 2; ++y) {
        for (size_t x = 0; x < 2; ++x) {
            const uint8_t *p = frame->data[0] + y * frame->stride[0] + 4 * x;
            mulib_img_rgba linear = {
                (float)mulib_color_transfer_decode(MULIB_COLOR_TF_SRGB, p[0] / 255.0),
                (float)mulib_color_transfer_decode(MULIB_COLOR_TF_SRGB, p[1] / 255.0),
                (float)mulib_color_transfer_decode(MULIB_COLOR_TF_SRGB, p[2] / 255.0),
                p[3] / 255.0f /* Alpha is not transfer encoded. */
            };
            mulib_img_put(fg, x, y, linear);
            mulib_img_put(bg, x, y, (mulib_img_rgba){0.0f, 0.0f, 0.25f, 1.0f});
        }
    }
    const mulib_color_grade_cdl grade = {{0.5f, 1, 1}, {0, 0, 0}, {1, 1, 1}, 1};
    assert(mulib_color_grade_apply(fg, fg, grade));
    assert(mulib_img_blend(dst, bg, fg, MULIB_IMG_OVER));
    for (size_t y = 0; y < 2; ++y) {
        for (size_t x = 0; x < 2; ++x) {
            mulib_img_rgba p = mulib_img_get(dst, x, y);
            assert(fabsf(p.a - 1.0f) <= FLT_EPSILON);
            const float channels[] = {p.r, p.g, p.b};
            for (size_t c = 0; c < 3; ++c) {
                double encoded = mulib_color_transfer_encode(MULIB_COLOR_TF_SRGB, channels[c]);
                assert(isfinite(encoded) && encoded >= 0 && encoded <= 1);
                rgb[(2 * y + x) * 3 + c] = (uint8_t)lround(255 * encoded);
            }
        }
    }
}

int
main(int argc, char **argv)
{
    uint8_t frames[2][24] = {
        {0,255,0,255, 255,0,0,255, 99,99,99,99,
         0,0,255,255, 255,255,255,255, 99,99,99,99},
        {255,0,0,128, 255,255,0,0, 99,99,99,99,
         255,0,0,255, 128,0,0,255, 99,99,99,99}
    };
    mulib_nle_item items[2];
    mulib_nle_timeline tl = {items, 0, 2, {1001, 30000}};
    assert(mulib_nle_timeline_insert(&tl, (mulib_nle_item){1, 2, 3, 20, 2, 0}));
    assert(mulib_nle_edit_split(&tl, 1, 21, 4));
    assert(mulib_nle_ripple_shift(&tl, 3, 21, 2));
    selection s = {0};
    assert(mulib_nle_evaluate_at(&tl, 21, select_frame, &s) && s.count == 0);
    assert(mulib_nle_evaluate_at(&tl, 23, select_frame, &s));
    assert(s.count == 1 && s.id == 4 && s.source == 1);
    int64_t audio_tick;
    assert(mulib_media_timebase_rescale(23, tl.timebase, (mulib_media_rational){1,48000},
                                       &audio_tick));
    assert(audio_tick == 36836); /* 23 * 1001 * 48000 / 30000, truncated. */

    mulib_media_frame frame = {0};
    frame.width = frame.height = 2;
    frame.format = MULIB_MEDIA_RGBA8;
    frame.data[0] = frames[s.source];
    frame.stride[0] = 12;
    frame.capacity[0] = 19;
    assert(!mulib_media_frame_valid(&frame));
    frame.capacity[0] = 20; /* No final row padding required. */
    uint8_t rgb[12];
    render(&frame, rgb);
    /* Independent reference: 1st pixel is (64/255,0,127/1020) in light;
     * the fully transparent yellow contributes nothing, even after grade. */
    const uint8_t expected[] = {137,0,99, 0,0,137, 188,0,0, 92,0,0};
    assert(!memcmp(rgb, expected, sizeof rgb));
    if (argc == 2 && !strcmp(argv[1], "--rgb24")) {
        assert(fwrite(rgb, 1, sizeof rgb, stdout) == sizeof rgb);
    } else {
        assert(argc == 1);
        puts("PASS media integration: split/ripple, NTSC timing, strided frame, linear grade/over");
    }
    return 0;
}
