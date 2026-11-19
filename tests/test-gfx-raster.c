/* Headless regression tests for the shared surface/ordered CPU raster layer. */
#include <stdlib.h>

/* Deterministic allocation failure injection for the header-only code. */
static long allocation_budget = -1;

static int allocation_allowed(void)
{
    if (allocation_budget < 0)
        return 1;
    if (allocation_budget == 0)
        return 0;
    allocation_budget--;
    return 1;
}

static void *raster_malloc(size_t size)
{
    return allocation_allowed() ? malloc(size) : NULL;
}

static void *raster_calloc(size_t count, size_t size)
{
    return allocation_allowed() ? calloc(count, size) : NULL;
}

static void *raster_realloc(void *p, size_t size)
{
    return allocation_allowed() ? realloc(p, size) : NULL;
}

#define malloc raster_malloc
#define calloc raster_calloc
#define realloc raster_realloc
#include "mulib-glx-clip.h"
#include "mulib-glx-compositor.h"
#include "mulib-glx-pixelformat.h"
#include "mulib-glx-region.h"
#include "mulib-glx-renderqueue.h"
#undef malloc
#undef calloc
#undef realloc
#include <assert.h>
#include <float.h>

static uint32_t random_state = UINT32_C(0x4d754c49);

static uint32_t next_random(void)
{
    random_state ^= random_state << 13;
    random_state ^= random_state >> 17;
    random_state ^= random_state << 5;
    return random_state;
}

static int rect_equal(mgx_rect a, mgx_rect b)
{
    return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

/* Independent double-precision straight-alpha reference. */
static uint32_t reference_blend(uint32_t s, uint32_t d,
                                mulib_glx_blend_mode mode)
{
    double a = (s >> 24) / 255., b = (d >> 24) / 255.;
    double alpha = a + b * (1 - a);
    uint32_t out;
    if (mode == MULIB_GLX_SOURCE)
        return s;
    if (alpha == 0)
        return 0;
    out = (uint32_t)floor(alpha * 255 + .5) << 24;
    for (int k = 0; k < 24; k += 8) {
        double x = (s >> k) & 255u, y = (d >> k) & 255u;
        double blend = x;
        if (mode == MULIB_GLX_MULTIPLY)
            blend = floor(x * y / 255 + .5);
        else if (mode == MULIB_GLX_SCREEN)
            blend = 255 - floor((255 - x) * (255 - y) / 255 + .5);
        else if (mode == MULIB_GLX_ADD)
            blend = fmin(255, x + y);
        out |= (uint32_t)floor((x * a * (1 - b) + y * b * (1 - a) +
                               blend * a * b) / alpha + .5 + 1e-10) << k;
    }
    return out;
}

static void test_capacity_and_views(void)
{
    uint32_t pixels[12], before[12];
    mgx_surface s = {0}, view = {0}, copy = {0}, bad;
    size_t words = 0;
    for (size_t i = 0; i < 12; i++)
        pixels[i] = UINT32_C(0x12345678);
    assert(mgx_surface_required_words(5, 3, 2, &words) && words == 8);
    assert(!mgx_surface_required_words(SIZE_MAX, 2, 2, &words));
    assert(!mgx_surface_required_words(1, 2, 1, &words));
    assert(!mgx_surface_required_words(5, 3, 0, &words));
    assert(!mgx_surface_required_words(5, 3, 2, NULL));
    assert(mulib_glx_surface_borrow_capacity(&s, pixels + 1, 32, 20, 3, 2));
    bad = s;
    assert(!mulib_glx_surface_borrow_capacity(&s, pixels + 1, 31, 20, 3, 2));
    assert(s.pixels == bad.pixels && s.stride == bad.stride);
    assert(!mulib_glx_surface_borrow_capacity(&s, pixels, 48, 19, 3, 2));
    assert(!mgx_wrap_capacity(&s, pixels, 12, SIZE_MAX, 3, 2));
    assert(!mgx_wrap(&s, (uint32_t *)((unsigned char *)pixels + 1), 3, 3, 1));
    assert(!mgx_wrap(NULL, pixels, 3, 3, 1));
    mgx_clear(&s, UINT32_C(0xffabcdef));
    for (size_t i = 0; i < 12; i++)
        assert(pixels[i] == ((i >= 1 && i <= 3) || (i >= 6 && i <= 8) ?
                             UINT32_C(0xffabcdef) : UINT32_C(0x12345678)));
    assert(mulib_glx_surface_view(&view, &s, (mgx_rect){2, 1, 50, 50}));
    assert(view.pixels == pixels + 8 && view.w == 1 && view.h == 1);
    assert(!view.owned && mulib_glx_surface_overlaps(&view, &s));
    assert(!mulib_glx_surface_view(&s, &s, (mgx_rect){0, 0, 1, 1}));
    assert(mulib_glx_surface_snapshot(&copy, &s));
    assert(copy.owned && !mulib_glx_surface_overlaps(&copy, &s));
    mgx_surface_free(&view);
    assert(pixels[8] == UINT32_C(0xffabcdef));
    memcpy(before, pixels, sizeof pixels);
    bad = s;
    bad.stride = 1;
    mgx_clear(&bad, 0);
    mgx_fill(&bad, (mgx_rect){0, 0, 3, 2}, 0);
    mgx_pixel(&bad, 0, 0, 0);
    assert(memcmp(before, pixels, sizeof pixels) == 0);
    mgx_surface_free(&copy);
    mgx_surface_free(&s);
    assert(memcmp(before, pixels, sizeof pixels) == 0);
}

static void test_clips_and_extremes(void)
{
    uint32_t pixels[16] = {0};
    mgx_surface s = {0};
    mulib_glx_clipstack_t clips = {0};
    mgx_rect original = {-5, -7, INT_MAX, INT_MAX};
    assert(mgx_wrap_capacity(&s, pixels + 1, 14, 5, 3, 3));
    s.clip = original;
    assert(mulib_glx_clip_push(&clips, &s, (mgx_rect){1, 1, 1, 1}));
    mgx_fill(&s, (mgx_rect){INT_MIN, INT_MIN, INT_MAX, INT_MAX}, 0xffffffffu);
    assert(pixels[7] == 0);
    mgx_fill(&s, (mgx_rect){0, 0, 3, 3}, 0xffffffffu);
    assert(pixels[7] == 0xffffffffu);
    assert(mulib_glx_clip_push(&clips, &s, (mgx_rect){2, 2, 1, 1}));
    assert(s.clip.w == 0 && s.clip.h == 0);
    assert(mulib_glx_clip_pop(&clips, &s));
    assert(mulib_glx_clip_pop(&clips, &s));
    assert(rect_equal(s.clip, original));
    assert(!mulib_glx_clip_pop(&clips, &s));
    for (int i = 0; i < 64; i++)
        assert(mulib_glx_clip_push(&clips, &s, (mgx_rect){0, 0, 3, 3}));
    assert(!mulib_glx_clip_push(&clips, &s, original));
    for (int i = 0; i < 64; i++)
        assert(mulib_glx_clip_pop(&clips, &s));
    clips.n = 65;
    assert(!mulib_glx_clip_push(&clips, &s, original));
    assert(!mulib_glx_clip_pop(&clips, &s));
    s.clip = (mgx_rect){INT_MAX - 1, INT_MAX - 1, INT_MAX, INT_MAX};
    mgx_line(&s, (mgx_point){-DBL_MAX, 0}, (mgx_point){DBL_MAX, 0},
             DBL_MAX, 0xff123456u);
    mgx_circle(&s, 0, 0, DBL_MAX, 0xff123456u);
    s.clip = original;
    mgx_circle(&s, 0, 0, DBL_MAX, 0xff123456u);
    for (int y = 0; y < 3; y++)
        for (int x = 0; x < 3; x++)
            assert(pixels[1 + y * 5 + x] == 0xff123456u);
    mgx_line(&s, (mgx_point){NAN, 1}, (mgx_point){2, 2}, 2, 0xffffffffu);
    mgx_circle(&s, INFINITY, 1, 2, 0xffffffffu);
    assert(pixels[0] == 0 && pixels[4] == 0 && pixels[5] == 0 &&
           pixels[9] == 0 && pixels[10] == 0 && pixels[14] == 0 && pixels[15] == 0);
    assert(mgx_int(INFINITY) == INT_MAX && mgx_int(-INFINITY) == INT_MIN);
    assert(mgx_int(NAN) == 0);
}

static void test_blending(void)
{
    assert(mgx_over(0x800000ffu, 0x80ff0000u) == 0xc05500aau);
    for (int mode = MULIB_GLX_OVER; mode <= MULIB_GLX_ADD; mode++) {
        for (unsigned a = 0; a < 256; a++) {
            for (unsigned b = 0; b < 256; b++) {
                uint32_t s = (a << 24) | 0x00d34017u;
                uint32_t d = (b << 24) | 0x002963e1u;
                assert(mulib_glx_blend(s, d, (mulib_glx_blend_mode)mode) ==
                       reference_blend(s, d, (mulib_glx_blend_mode)mode));
            }
        }
    }
    assert(mulib_glx_blend(0x80abcdefu, 0, MULIB_GLX_MULTIPLY) == 0x80abcdefu);
    assert(mulib_glx_blend(0x12345678u, 0xffabcdefu,
                          (mulib_glx_blend_mode)99) ==
           mgx_over(0x12345678u, 0xffabcdefu));
}

static void test_texture_sampling(void)
{
    uint32_t pixels[] = {0xffff0000u, 0x000000ffu};
    uint32_t opaque[] = {0xff123456u, 0xffabcdefu, 0xff102030u};
    uint32_t target[3] = {0};
    mgx_surface s = {0}, d = {0}, bad = {0};
    assert(mgx_wrap(&s, pixels, 2, 2, 1));
    assert(mulib_glx_texture_sample(&s, .5, 0, 1) == 0x80ff0000u);
    assert(mulib_glx_texture_sample(&s, -1, 3, 0) == pixels[0]);
    assert(mulib_glx_texture_sample(&s, 4, 0, 0) == pixels[1]);
    assert(!mulib_glx_texture_sample(&s, NAN, 0, 0));
    assert(!mulib_glx_texture_sample(&bad, 0, 0, 1));
    assert(mgx_wrap(&s, opaque, 3, 3, 1));
    assert(mgx_wrap(&d, target, 3, 3, 1));
    assert(mulib_glx_texture_draw_checked(&d, &s, (mgx_rect){0, 0, 3, 1}, 1));
    assert(memcmp(opaque, target, sizeof opaque) == 0);
    assert(mulib_glx_texture_draw_checked(&d, &d, (mgx_rect){0, 0, 3, 1}, 1));
    assert(memcmp(opaque, target, sizeof opaque) == 0);
    assert(!mulib_glx_texture_draw_checked(&bad, &s, (mgx_rect){0, 0, 3, 1}, 1));
}

static int random_coordinate(void)
{
    uint32_t r = next_random();
    if ((r & 15u) == 0)
        return INT_MIN;
    if ((r & 15u) == 1)
        return INT_MAX;
    return (int)(r % 101u) - 50;
}

static void test_rectangle_and_primitive_properties(void)
{
    for (int iteration = 0; iteration < 2000; iteration++) {
        mgx_rect a = {random_coordinate(), random_coordinate(),
                      random_coordinate(), random_coordinate()};
        mgx_rect b = {random_coordinate(), random_coordinate(),
                      random_coordinate(), random_coordinate()};
        mgx_rect r = mgx_intersect(a, b);
        int64_t left = a.x > b.x ? a.x : b.x;
        int64_t top = a.y > b.y ? a.y : b.y;
        int64_t aright = (int64_t)a.x + (a.w > 0 ? a.w : 0);
        int64_t abottom = (int64_t)a.y + (a.h > 0 ? a.h : 0);
        int64_t bright = (int64_t)b.x + (b.w > 0 ? b.w : 0);
        int64_t bbottom = (int64_t)b.y + (b.h > 0 ? b.h : 0);
        int64_t right = aright < bright ? aright : bright;
        int64_t bottom = abottom < bbottom ? abottom : bbottom;
        uint32_t pixels[24];
        mgx_surface s = {0};
        for (size_t i = 0; i < 24; i++)
            pixels[i] = 0x19384756u;
        assert(r.x == left && r.y == top);
        assert(r.w == (right > left ? right - left : 0));
        assert(r.h == (bottom > top ? bottom - top : 0));
        assert(rect_equal(r, mgx_intersect(b, a)));
        assert(mgx_wrap_capacity(&s, pixels + 1, 23, 7, 3, 3));
        s.clip = a;
        mgx_fill(&s, b, 0xffabcdefu);
        for (int y = 0; y < 3; y++) {
            for (int x = 0; x < 3; x++) {
                int in_a = a.w > 0 && a.h > 0 && x >= a.x && y >= a.y &&
                    (int64_t)x < (int64_t)a.x + a.w &&
                    (int64_t)y < (int64_t)a.y + a.h;
                int in_b = b.w > 0 && b.h > 0 && x >= b.x && y >= b.y &&
                    (int64_t)x < (int64_t)b.x + b.w &&
                    (int64_t)y < (int64_t)b.y + b.h;
                assert(pixels[1 + y * 7 + x] ==
                       (in_a && in_b ? 0xffabcdefu : 0x19384756u));
            }
        }
        mgx_line(&s, (mgx_point){a.x, a.y}, (mgx_point){b.x, b.y},
                 iteration % 7 == 0 ? DBL_MAX : 2, 0x8000ff00u);
        mgx_circle(&s, a.x, a.y,
                   iteration % 11 == 0 ? DBL_MAX : 10, 0x800000ffu);
        for (size_t i = 0; i < 24; i++) {
            int active = (i >= 1 && i <= 3) || (i >= 8 && i <= 10) ||
                         (i >= 15 && i <= 17);
            if (!active)
                assert(pixels[i] == 0x19384756u);
        }
    }
}

/* Compare aliased, differently strided/base-offset views with execution using an
 * explicitly independent source copy. Compare all storage, including padding.
 */
static void test_overlapping_blits(void)
{
    for (int iteration = 0; iteration < 800; iteration++) {
        uint32_t actual[80], expected[80];
        mgx_surface src = {0}, dst = {0}, refdst = {0}, snapshot = {0};
        int x = (int)(next_random() % 15) - 7;
        int y = (int)(next_random() % 13) - 6;
        mgx_rect clip = {(int)(next_random() % 9) - 3,
                         (int)(next_random() % 9) - 3, 7, 7};
        for (size_t i = 0; i < 80; i++)
            actual[i] = next_random();
        memcpy(expected, actual, sizeof actual);
        assert(mgx_wrap_capacity(&src, actual + 8, 72, 9, 5, 4));
        assert(mgx_wrap_capacity(&dst, actual + 9, 71, 8, 6, 5));
        assert(mgx_wrap_capacity(&refdst, expected + 9, 71, 8, 6, 5));
        dst.clip = refdst.clip = clip;
        src.clip = (mgx_rect){0, 0, 0, 0}; /* Sampling ignores drawing clips. */
        assert(mulib_glx_surface_snapshot(&snapshot, &src));
        if (iteration & 1) {
            mulib_glx_blend_mode mode =
                (mulib_glx_blend_mode)(iteration % (MULIB_GLX_ADD + 1));
            double opacity = (iteration % 4) * .4;
            assert(mulib_glx_composite_checked(&dst, &src, x, y, opacity, mode));
            assert(mulib_glx_composite_checked(&refdst, &snapshot, x, y,
                                               opacity, mode));
        } else {
            mgx_rect rect = {x, y, 1 + iteration % 10, 1 + iteration % 8};
            assert(mulib_glx_texture_draw_checked(&dst, &src, rect, iteration & 2));
            assert(mulib_glx_texture_draw_checked(&refdst, &snapshot, rect,
                                                  iteration & 2));
        }
        assert(memcmp(actual, expected, sizeof actual) == 0);
        assert(rect_equal(dst.clip, clip));
        mgx_surface_free(&snapshot);
    }
    {
        uint32_t p[4] = {1, 2, 3, 4};
        mgx_surface s = {0};
        assert(mgx_wrap(&s, p, 2, 2, 2));
        assert(mulib_glx_composite_checked(&s, &s, 0, 0, 1, MULIB_GLX_SOURCE));
        assert(p[0] == 1 && p[1] == 2 && p[2] == 3 && p[3] == 4);
        assert(!mulib_glx_composite_checked(&s, &s, 0, 0, NAN, MULIB_GLX_SOURCE));
        assert(!mulib_glx_composite_checked(&s, &s, 0, 0, 1,
                                           (mulib_glx_blend_mode)99));
        assert(mulib_glx_composite_checked(&s, &s, INT_MIN, INT_MAX, 1,
                                           MULIB_GLX_SOURCE));
    }
}

static void test_pixel_conversion(void)
{
    uint32_t colors[] = {0x00ff0000u, 0x8000ff00u, 0xff0000ffu,
                         0x40123456u, 0x78abcdefu, 0xff102030u};
    unsigned char packed[64], unpacked[64], saved[64];
    for (int format = MGX_ARGB8888; format <= MGX_RGB565; format++) {
        size_t bpp = mulib_glx_pixelformat_bytes((mulib_glx_pixelformat_t)format);
        size_t pitch = 3 * bpp + 3;
        memset(packed, 0x5a, sizeof packed);
        memset(unpacked, 0x6b, sizeof unpacked);
        assert(mulib_glx_pixelformat_convert(packed + 1, 63, pitch,
            (mulib_glx_pixelformat_t)format, colors, sizeof colors, 12,
            MGX_ARGB8888, 3, 2));
        assert(mulib_glx_pixelformat_convert(unpacked + 1, 63, 15,
            MGX_ARGB8888, packed + 1, 63, pitch,
            (mulib_glx_pixelformat_t)format, 3, 2));
        for (int y = 0; y < 2; y++) {
            for (int x = 0; x < 3; x++) {
                uint32_t c;
                memcpy(&c, unpacked + 1 + y * 15 + x * 4, sizeof c);
                assert(c == mulib_glx_pixelformat_unpack(
                    mulib_glx_pixelformat_pack(colors[y * 3 + x],
                        (mulib_glx_pixelformat_t)format),
                    (mulib_glx_pixelformat_t)format));
            }
        }
        assert(packed[0] == 0x5a && packed[1 + 3 * bpp] == 0x5a);
        assert(unpacked[0] == 0x6b && unpacked[13] == 0x6b);
    }
    memset(packed, 0x5a, sizeof packed);
    memcpy(saved, packed, sizeof saved);
    assert(!mulib_glx_pixelformat_convert(packed, 23, 12, MGX_ARGB8888,
        colors, sizeof colors, 12, MGX_ARGB8888, 3, 2));
    assert(!mulib_glx_pixelformat_convert(packed, 64, SIZE_MAX, MGX_ARGB8888,
        colors, sizeof colors, 12, MGX_ARGB8888, 3, 2));
    assert(!mulib_glx_pixelformat_convert(packed, 64, 12,
        (mulib_glx_pixelformat_t)99, colors, sizeof colors, 12, MGX_ARGB8888, 3, 2));
    assert(memcmp(packed, saved, sizeof packed) == 0);
    memcpy(packed, colors, sizeof colors);
    assert(mulib_glx_pixelformat_convert(packed + 3, 61, 8, MGX_RGB565,
        packed, 64, 12, MGX_ARGB8888, 3, 2));
    assert(mulib_glx_pixelformat_convert(packed, 64, 12, MGX_ARGB8888,
        packed + 3, 61, 8, MGX_RGB565, 3, 2));
    for (int i = 0; i < 6; i++) {
        uint32_t c;
        memcpy(&c, packed + i * 4, sizeof c);
        assert(c == mulib_glx_pixelformat_unpack(
            mulib_glx_pixelformat_pack(colors[i], MGX_RGB565), MGX_RGB565));
    }
}

static mgx_drawcmd rect_command(uint32_t color)
{
    mgx_drawcmd c = {0};
    c.kind = MGX_RECT;
    c.bounds = c.clip = (mgx_rect){0, 0, 2, 2};
    c.color = color;
    return c;
}

static void test_order_and_snapshots(void)
{
    mgx_surface s = {0}, image = {0};
    mulib_glx_drawlist_t first = {0}, second = {0}, images = {0};
    mulib_glx_renderqueue_t q = {0};
    mulib_glx_drawlist_snapshot_t frozen = {0};
    mgx_drawcmd c;
    mgx_rect old = {-1, -1, INT_MAX, INT_MAX};
    assert(mgx_surface_init(&s, 2, 2));
    s.clip = old;
    assert(mulib_glx_drawlist_add(&first, rect_command(0x80ff0000u)));
    assert(mulib_glx_drawlist_add(&second, rect_command(0x800000ffu)));
    assert(!mulib_glx_renderqueue_submit(NULL, &first));
    assert(mulib_glx_renderqueue_submit(&q, &first));
    assert(mulib_glx_renderqueue_submit(&q, &second));
    assert(mulib_glx_renderqueue_execute_checked(&q, &s) && q.n == 0);
    for (int i = 0; i < 4; i++)
        assert(s.pixels[i] == 0xc05500aau);
    assert(rect_equal(s.clip, old));
    c = rect_command(0xffffffffu);
    c.kind = (mgx_draw_kind)99;
    assert(!mulib_glx_drawlist_add(&first, c) && first.n == 1);
    c.kind = MGX_LINE;
    c.width = NAN;
    assert(!mulib_glx_drawlist_add(&first, c));
    assert(mulib_glx_renderqueue_submit(&q, &first));
    first.v[0].kind = (mgx_draw_kind)99;
    assert(!mulib_glx_renderqueue_execute_checked(&q, &s) && q.n == 1);
    assert(s.pixels[0] == 0xc05500aau && rect_equal(s.clip, old));
    first.v[0].kind = MGX_RECT;
    assert(mulib_glx_renderqueue_execute_checked(&q, &s));
    assert(mgx_surface_init(&image, 2, 1));
    image.pixels[0] = 0xff123456u;
    image.pixels[1] = 0xffabcdefu;
    c = (mgx_drawcmd){0};
    c.kind = MGX_IMAGE;
    c.image = &image;
    c.bounds = (mgx_rect){0, 0, 2, 1};
    c.clip = (mgx_rect){0, 0, 2, 2};
    assert(mulib_glx_drawlist_add(&images, c));
    c.bounds.y = 1;
    assert(mulib_glx_drawlist_add(&images, c));
    assert(mulib_glx_drawlist_snapshot(&frozen, &images));
    assert(frozen.image_count == 2);
    mgx_surface_free(&image);
    mulib_glx_drawlist_release(&images);
    assert(mulib_glx_renderqueue_submit(&q, &frozen.draws));
    assert(mulib_glx_renderqueue_execute_checked(&q, &s));
    assert(s.pixels[0] == 0xff123456u && s.pixels[1] == 0xffabcdefu);
    assert(s.pixels[2] == 0xff123456u && s.pixels[3] == 0xffabcdefu);
    assert(rect_equal(s.clip, old));
    mulib_glx_drawlist_snapshot_release(&frozen);
    mulib_glx_drawlist_release(&first);
    mulib_glx_drawlist_release(&second);
    mulib_glx_renderqueue_free(&q);
    mgx_surface_free(&s);
    mulib_glx_drawlist_reset(NULL);
    mulib_glx_drawlist_release(NULL);
    mulib_glx_renderqueue_execute(NULL, NULL);
    mulib_glx_renderqueue_free(NULL);
}

static void test_regions(void)
{
    mulib_glx_region_t r = {0};
    assert(mulib_glx_region_add(&r, (mgx_rect){0, 0, 8, 8}));
    assert(mulib_glx_region_subtract(&r, (mgx_rect){2, 2, 4, 4}));
    assert(r.n == 4);
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            int count = 0;
            for (size_t i = 0; i < r.n; i++)
                count += mgx_contains(r.v[i], x, y);
            assert(count == !(x >= 2 && x < 6 && y >= 2 && y < 6));
        }
    }
    mulib_glx_region_clip(&r, (mgx_rect){0, 0, 2, 8});
    assert(r.n == 3);
    mulib_glx_region_free(&r);
    assert(mulib_glx_region_add(&r, (mgx_rect){INT_MAX - 2, INT_MAX - 2, 3, 3}));
    assert(!mulib_glx_region_add(&r, (mgx_rect){INT_MAX, 0, 2, 1}));
    assert(r.n == 1);
    assert(mulib_glx_region_subtract(&r, (mgx_rect){INT_MAX, INT_MAX, 1, 1}));
    assert(r.n == 2);
    assert(mulib_glx_region_subtract(&r,
        (mgx_rect){INT_MAX - 2, INT_MAX - 2, 3, 3}));
    assert(r.n == 0);
    assert(mulib_glx_region_add(&r, (mgx_rect){INT_MIN, INT_MIN, INT_MAX, INT_MAX}));
    assert(mulib_glx_region_subtract(&r, (mgx_rect){-5, -5, INT_MAX, INT_MAX}));
    assert(r.n == 2);
    mulib_glx_region_free(&r);
    assert(!mulib_glx_region_subtract(NULL, (mgx_rect){0, 0, 1, 1}));
    mulib_glx_region_clip(NULL, (mgx_rect){0, 0, 1, 1});
    mulib_glx_region_free(NULL);
}

static void test_allocation_failures(void)
{
    uint32_t pixels[8], before[8], converted[8];
    mgx_surface s = {0}, unchanged = {0}, other = {0};
    mulib_glx_drawlist_t draws = {0};
    mulib_glx_drawlist_snapshot_t frozen = {0};
    mulib_glx_renderqueue_t queue = {0};
    mulib_glx_region_t region = {0};
    mgx_drawcmd c = {0};
    mgx_rect original;
    size_t old_cap;
    mgx_rect *old_v;
    void *p = NULL;
    size_t capacity = 0;
    for (size_t i = 0; i < 8; i++)
        pixels[i] = 0xff123456u + (uint32_t)i;
    memcpy(before, pixels, sizeof pixels);
    assert(mgx_wrap_capacity(&s, pixels, 8, 4, 3, 2));
    original = s.clip;
    allocation_budget = 0;
    assert(!mgx_surface_init(&unchanged, 3, 2) && !unchanged.pixels);
    assert(!mulib_glx_surface_snapshot(&unchanged, &s) && !unchanged.pixels);
    assert(!mulib_glx_texture_draw_checked(&s, &s, (mgx_rect){0, 0, 3, 2}, 1));
    assert(!mulib_glx_composite_checked(&s, &s, 1, 0, 1, MULIB_GLX_OVER));
    assert(memcmp(before, pixels, sizeof pixels) == 0);
    assert(!mulib_glx_pixelformat_convert(pixels + 1, 28, 12, MGX_ARGB8888,
        pixels, 32, 12, MGX_ARGB8888, 3, 2));
    assert(memcmp(before, pixels, sizeof pixels) == 0);
    assert(!mgx_grow(&p, &capacity, 1, sizeof(uint32_t)));
    assert(!p && capacity == 0);
    assert(!mgx_grow(NULL, &capacity, 1, 1));
    assert(!mgx_grow(&p, NULL, 1, 1));
    assert(!mgx_grow(&p, &capacity, SIZE_MAX, 4));
    assert(!mulib_glx_drawlist_add(&draws, rect_command(0xffffffffu)));
    assert(draws.n == 0 && draws.cap == 0 && !draws.v);
    allocation_budget = -1;
    c.kind = MGX_IMAGE;
    c.bounds = c.clip = (mgx_rect){0, 0, 3, 2};
    c.image = &s;
    assert(mulib_glx_drawlist_add(&draws, c));
    /* Copy-array, image-array, and image-pixel allocation failures. */
    for (long budget = 0; budget < 3; budget++) {
        allocation_budget = budget;
        assert(!mulib_glx_drawlist_snapshot(&frozen, &draws));
        assert(!frozen.draws.v && !frozen.images && frozen.image_count == 0);
    }
    allocation_budget = -1;
    assert(mulib_glx_renderqueue_submit(&queue, &draws));
    allocation_budget = 0;
    assert(!mulib_glx_renderqueue_execute_checked(&queue, &s));
    assert(queue.n == 0 && rect_equal(s.clip, original));
    assert(memcmp(before, pixels, sizeof pixels) == 0);
    allocation_budget = -1;
    mulib_glx_drawlist_reset(&draws);
    assert(mulib_glx_drawlist_add(&draws, rect_command(0xff102030u)));
    assert(mulib_glx_drawlist_add(&draws, c));
    assert(mulib_glx_renderqueue_submit(&queue, &draws));
    allocation_budget = 0;
    assert(!mulib_glx_renderqueue_execute_checked(&queue, &s));
    assert(queue.n == 0 && rect_equal(s.clip, original));
    assert(pixels[0] == 0xff102030u && pixels[1] == 0xff102030u);
    assert(pixels[4] == 0xff102030u && pixels[5] == 0xff102030u);
    assert(pixels[2] == before[2] && pixels[6] == before[6]);
    /* Nonaliasing drawing does not require an allocation. */
    assert(mgx_wrap(&other, converted, 4, 3, 2));
    memset(converted, 0, sizeof converted);
    assert(mulib_glx_composite_checked(&other, &s, 0, 0, 1, MULIB_GLX_SOURCE));
    assert(converted[0] == pixels[0] && converted[6] == pixels[6]);
    allocation_budget = -1;
    assert(mulib_glx_region_add(&region, (mgx_rect){0, 0, 8, 8}));
    old_v = region.v;
    old_cap = region.cap;
    allocation_budget = 0;
    assert(!mulib_glx_region_subtract(&region, (mgx_rect){2, 2, 4, 4}));
    assert(region.v == old_v && region.n == 1 && region.cap == old_cap);
    assert(rect_equal(region.v[0], (mgx_rect){0, 0, 8, 8}));
    allocation_budget = -1;
    mulib_glx_region_free(&region);
    mulib_glx_drawlist_release(&draws);
    mulib_glx_renderqueue_free(&queue);
}

int main(void)
{
    test_capacity_and_views();
    test_clips_and_extremes();
    test_blending();
    test_texture_sampling();
    test_rectangle_and_primitive_properties();
    test_overlapping_blits();
    test_pixel_conversion();
    test_order_and_snapshots();
    test_regions();
    test_allocation_failures();
    puts("gfx raster: capacity, clips, blend, overlap, formats, ordering, lifetime, OOM OK");
    return 0;
}
