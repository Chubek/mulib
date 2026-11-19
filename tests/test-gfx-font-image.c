/* Standalone, dependency-free font/image regression and allocation failures. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t allocation_budget = SIZE_MAX;

static int
allocation_fails(void)
{
    if (allocation_budget == SIZE_MAX)
        return 0;
    if (!allocation_budget)
        return 1;
    --allocation_budget;
    return 0;
}

static void *
test_malloc(size_t n)
{
    return allocation_fails() ? NULL : malloc(n);
}

static void *
test_calloc(size_t n, size_t size)
{
    return allocation_fails() ? NULL : calloc(n, size);
}

#define malloc test_malloc
#define calloc test_calloc
#include "mulib-glx-font.h"
#include "mulib-glx-fontscan.h"
#include "mulib-glx-glyphcache.h"
#include "mulib-glx-text.h"
#include "mulib-glx-textatlas.h"
#include "mulib-glx-icon.h"
#include "mulib-glx-imagedecode.h"
#undef malloc
#undef calloc

static void
put32(unsigned char *p, uint32_t v)
{
    p[0] = (unsigned char)v;
    p[1] = (unsigned char)(v >> 8);
    p[2] = (unsigned char)(v >> 16);
    p[3] = (unsigned char)(v >> 24);
}

/* Six original two-row glyphs and UTF-8 aliases, not a vendor font asset. */
static size_t
make_psf(unsigned char *p)
{
    static const unsigned char table[] = {
        0xef, 0xbf, 0xbd, 0xff,                         /* replacement */
        '?', 0xff,
        'A', 0xc3, 0xa9, 0xf0, 0x9f, 0x99, 0x82, 0xff,
        0xe4, 0xb8, 0xad, 0xfe, 'f', 'i',
        0xfe, 'a', 0xcc, 0x81, 0xff,                   /* two sequences */
        0xfe, 'f', 'l', 0xff,                          /* sequence only */
        'A', 0xf4, 0x8f, 0xbf, 0xbf, 0xff             /* duplicate + max */
    };
    memset(p, 0, 128);
    put32(p, 0x864ab572);
    put32(p + 8, 32);
    put32(p + 12, 1);
    put32(p + 16, 6);
    put32(p + 20, 4);
    put32(p + 24, 2);
    put32(p + 28, 9);
    for (unsigned i = 0; i < 5; ++i)
        p[32 + i * 4] = p[34 + i * 4] = (unsigned char)(0x80u >> i);
    p[32 + 5 * 4 + 1] = p[32 + 5 * 4 + 3] = 0x80;
    memcpy(p + 56, table, sizeof table);
    return 56 + sizeof table;
}

static void
font_rejected(const unsigned char *p, size_t n)
{
    mulib_glx_font_t f = {0};
    assert(!mulib_glx_font_psf2(&f, p, n));
    assert(!f.bits && !f.mapping && !f.count);
    mulib_glx_font_free(&f);
}

static void
test_psf(void)
{
    unsigned char p[128], bad[128];
    size_t n = make_psf(p);
    mulib_glx_font_t f = {0};
    uint32_t glyph = 99;
    for (size_t i = 0; i < n; ++i)
        font_rejected(p, i);
    assert(mulib_glx_font_psf2(&f, p, n));
    assert(f.count == 6 && f.width == 9 && f.height == 2 && f.unicode);
    assert(f.mapping_count == 7);
    assert(mulib_glx_font_lookup(&f, 'A', &glyph) && glyph == 2);
    assert(mulib_glx_font_lookup(&f, 0xe9, &glyph) && glyph == 2);
    assert(mulib_glx_font_lookup(&f, 0x1f642, &glyph) && glyph == 2);
    assert(mulib_glx_font_lookup(&f, 0x4e2d, &glyph) && glyph == 3);
    assert(mulib_glx_font_lookup(&f, 0x10ffff, &glyph) && glyph == 5);
    assert(mulib_glx_font_bit(&f, 0x10ffff, 8, 1));
    assert(!mulib_glx_font_bit(&f, 0x10ffff, 9, 1));
    assert(!mulib_glx_font_bit(&f, 0x10ffff, 8, 2));
    glyph = 42;
    assert(!mulib_glx_font_lookup(&f, 'f', &glyph) && glyph == 42);
    assert(!mulib_glx_font_lookup(&f, 'i', &glyph));
    assert(!mulib_glx_font_lookup(&f, 0x301, &glyph));
    assert(!mulib_glx_font_lookup(&f, 2, &glyph)); /* no identity aliases */
    assert(!mulib_glx_font_lookup(&f, 0xd800, &glyph));
    assert(!mulib_glx_font_lookup(&f, 0x110000, &glyph));
    assert(mulib_glx_font_glyph(&f, 'z') == 0);
    assert(mulib_glx_font_glyph(&f, 0xffffffff) == 0);
    assert(!mulib_glx_font_psf2(&f, p, n));        /* no owning overwrite */
    assert(!mulib_glx_font_builtin(&f));
    assert(mulib_glx_font_bit(&f, 0xe9, 2, 0));
    memset(p, 0, sizeof p);                     /* copied source lifetime */
    assert(mulib_glx_font_bit(&f, 0xe9, 2, 0));
    mulib_glx_font_free(&f);
    mulib_glx_font_free(&f);
    mulib_glx_font_free(NULL);
    assert(mulib_glx_font_glyph(&f, 'A') == UINT32_MAX);

    n = make_psf(p);
    p[58] = 0xbc;                               /* U+FFFC, not U+FFFD */
    assert(mulib_glx_font_psf2(&f, p, n));
    assert(mulib_glx_font_glyph(&f, 'z') == 1);   /* '?' fallback */
    mulib_glx_font_free(&f);
    p[60] = '>';
    assert(mulib_glx_font_psf2(&f, p, n));
    assert(mulib_glx_font_glyph(&f, 'z') == 0);   /* glyph-zero fallback */
    mulib_glx_font_free(&f);

    n = make_psf(p);
    put32(p + 12, 0);                          /* no-table identity mode */
    assert(mulib_glx_font_psf2(&f, p, 56));
    assert(mulib_glx_font_glyph(&f, 2) == 2);
    assert(mulib_glx_font_glyph(&f, 100) == 0);
    mulib_glx_font_free(&f);
    n = make_psf(p);
    for (unsigned field = 0; field < 8; ++field) {
        static const uint32_t values[] = {
            0, 1, 31, 2, UINT32_MAX, 3, 0, 257
        };
        memcpy(bad, p, n);
        put32(bad + field * 4, values[field]);
        font_rejected(bad, n);
    }
    memcpy(bad, p, n);
    bad[n] = 0xff;                             /* surplus glyph record */
    font_rejected(bad, n + 1);
    memcpy(bad, p, n);
    bad[56] = 0xc0;                            /* overlong UTF-8 */
    font_rejected(bad, n);
    memcpy(bad, p, n);
    bad[56] = 0xed; bad[57] = 0xa0; bad[58] = 0x80; /* surrogate */
    font_rejected(bad, n);
    memcpy(bad, p, n);
    bad[63] = 0xff;                            /* truncated alias */
    font_rejected(bad, n);
    memcpy(bad, p, n);
    bad[n - 5] = 0xf4; bad[n - 4] = 0x90;     /* > U+10FFFF */
    font_rejected(bad, n);
    memcpy(bad, p, n);
    bad[56] = 0xfe; bad[57] = 'a'; bad[58] = 0xff; /* short sequence */
    font_rejected(bad, n);
    memcpy(bad, p, n);
    bad[56] = 0xfe; bad[57] = 0xfe;            /* empty sequence */
    font_rejected(bad, n);
    assert(!mulib_glx_font_psf2(NULL, p, n));
    font_rejected(NULL, n);
    font_rejected(p, MULIB_GLX_FONT_MAX_BYTES + 1);
    for (size_t budget = 0; budget < 2; ++budget) {
        allocation_budget = budget;            /* bitmap, then map */
        font_rejected(p, n);
        allocation_budget = SIZE_MAX;
    }
}

static size_t
nonzero(const mgx_surface *s)
{
    size_t n = 0;
    for (int y = 0; y < s->h; ++y)
        for (int x = 0; x < s->w; ++x)
            n += s->pixels[(size_t)y * s->stride + x] != 0;
    return n;
}

static void
test_text_and_atlas(void)
{
    unsigned char p[128];
    size_t n = make_psf(p);
    mulib_glx_font_t f = {0}, builtin = {0};
    mgx_surface s = {0}, raster = {0};
    mgx_point measure;
    uint32_t storage[32 * 8];
    mulib_glx_atlas_t a = {20, 8, 0, 0, 0, 1}, before;
    mgx_rect cell = {90, 91, 92, 93};
    assert(mulib_glx_font_psf2(&f, p, n));
    assert(mgx_surface_init(&s, 32, 16));
    assert(mulib_glx_glyphcache_raster(&raster, &f, 0xe9, 0x80123456));
    assert(raster.pixels[2] == 0x80123456 && nonzero(&raster) == 2);
    assert(!mulib_glx_glyphcache_raster(&raster, &f, 0xe9, 0xffffffff));
    mulib_glx_text_draw(&s, &f, 0, 0, "\xc3\xa9", 2, 2, 0xffffffff);
    assert(s.pixels[2] == 0xffffffff && nonzero(&s) == 2);
    mgx_clear(&s, 0);
    mulib_glx_text_draw(&s, &f, 0, 0, "\xf0\x9f\x99\x82", 4, 2, 0xffffffff);
    assert(s.pixels[2] == 0xffffffff && nonzero(&s) == 2);
    mgx_clear(&s, 0);
    mulib_glx_text_draw(&s, &f, 0, 0, "\xe4\xb8\xad", 3, 2, 0xffffffff);
    assert(s.pixels[3] == 0xffffffff && nonzero(&s) == 2);
    mgx_clear(&s, 0);
    mulib_glx_text_draw(&s, &f, 0, 0, "\xff" "A", 2, 2, 0xffffffff);
    assert(s.pixels[0] == 0xffffffff && s.pixels[11] == 0xffffffff);
    assert(nonzero(&s) == 4);
    measure = mulib_glx_text_measure(&f, "\xff" "A", 2, 2);
    assert(measure.x == 18 && measure.y == 2);
    measure = mulib_glx_text_measure(&f, "\xc3", 1, 2);
    assert(measure.x == 9 && measure.y == 2);
    measure = mulib_glx_text_measure(&f, NULL, 1, 2);
    assert(measure.x == 0 && measure.y == 0);
    measure = mulib_glx_text_measure(&f, "", 0, NAN);
    assert(measure.x == 0 && measure.y == 0);
    measure = mulib_glx_text_measure(&f, "", 0, INFINITY);
    assert(measure.x == 0 && measure.y == 0);
    mgx_clear(&s, 0);
    s.clip = (mgx_rect){2, 0, 1, 1};
    mulib_glx_text_draw(&s, &f, 0, 0, "A", 1, 2, 0xff00ff00);
    assert(nonzero(&s) == 1 && s.pixels[2] == 0xff00ff00);
    mgx_clear(&s, 0);
    s.clip = (mgx_rect){0, 0, s.w, s.h};
    mulib_glx_text_draw(&s, &f, INT_MAX, INT_MAX, "AA\nA", 4, 1024, ~0u);
    mulib_glx_text_draw(&s, &f, INT_MIN, INT_MIN, "AA\nA", 4, 1024, ~0u);
    mulib_glx_text_draw(&s, &f, 0, 0, NULL, 1, 2, ~0u);
    mulib_glx_text_draw(&s, &f, 0, 0, "A", 1, 0, ~0u);
    assert(!nonzero(&s));
    assert(!mulib_glx_textatlas_add(&a, NULL, &f, 'A', &cell));
    assert(cell.x == 90 && a.x == 0);
    mgx_surface_free(&s);

    for (size_t i = 0; i < sizeof storage / sizeof *storage; ++i)
        storage[i] = 0xff123456;
    assert(mgx_wrap(&s, storage, 32, 20, 8));
    s.clip = (mgx_rect){0, 0, 0, 0};
    assert(mulib_glx_textatlas_add(&a, &s, &f, 'A', &cell));
    assert(cell.x == 0 && cell.y == 0 && cell.w == 9 && cell.h == 2);
    assert(storage[0] == 0 && storage[2] == 0xffffffff);
    assert(storage[9] == 0xff123456 && storage[20] == 0xff123456);
    assert(mulib_glx_textatlas_add(&a, &s, &f, 0x4e2d, &cell));
    assert(cell.x == 10 && cell.y == 0 && storage[13] == 0xffffffff);
    assert(mulib_glx_textatlas_add(&a, &s, &f, 0x10ffff, &cell));
    assert(cell.x == 0 && cell.y == 3 && storage[3 * 32 + 8] == 0xffffffff);
    before = a;
    s.w = 19;                                 /* undersized backing surface */
    assert(!mulib_glx_textatlas_add(&a, &s, &f, 'A', &cell));
    assert(a.x == before.x && a.y == before.y && a.row == before.row);
    s.w = 20;
    s.stride = SIZE_MAX;
    assert(!mulib_glx_textatlas_add(&a, &s, &f, 'A', &cell));
    assert(a.x == before.x && a.y == before.y && a.row == before.row);
    s.stride = 32;
    mulib_glx_font_free(&f);
    assert(raster.pixels[2] == 0x80123456);      /* no borrowed font lifetime */
    assert(storage[2] == 0xffffffff);
    mgx_surface_free(&raster);
    mgx_surface_free(&s);                       /* wrapped storage is borrowed */
    assert(storage[2] == 0xffffffff);

    a = (mulib_glx_atlas_t){INT_MAX, INT_MAX, 0, 0, 0, INT_MAX};
    assert(!mulib_glx_atlas_alloc(&a, 1, 1, &cell) && a.x == 0);
    a = (mulib_glx_atlas_t){10, 10, -1, 0, 0, 0};
    assert(!mulib_glx_atlas_alloc(&a, 1, 1, &cell) && a.x == -1);
    a = (mulib_glx_atlas_t){10, 10, 0, 9, 2, 0};
    assert(!mulib_glx_atlas_alloc(&a, 1, 1, &cell));
    a = (mulib_glx_atlas_t){10, 10, 0, 0, 0, 0};
    assert(mulib_glx_atlas_alloc(&a, 10, 10, &cell));
    before = a;
    cell = (mgx_rect){90, 91, 92, 93};
    assert(!mulib_glx_atlas_alloc(&a, 1, 1, &cell));
    assert(cell.x == 90 && a.x == before.x && a.y == before.y);

    allocation_budget = 0;
    assert(!mulib_glx_font_builtin(&builtin));
    allocation_budget = SIZE_MAX;
    assert(mulib_glx_font_builtin(&builtin));
    assert(mulib_glx_font_bit(&builtin, 'A', 1, 0));
    assert(!mulib_glx_font_bit(&builtin, 'A', 0, 0));
    assert(mulib_glx_font_bit(&builtin, 'A', 4, 3));
    assert(mulib_glx_font_glyph(&builtin, 0x1f642) == '?');
    assert(mgx_surface_init(&s, 20, 20));
    mulib_glx_text_draw(&s, &builtin, 0, 0, "A\r\nA", 4, 8, 0xffffffff);
    assert(nonzero(&s) == 36 && s.pixels[1] == 0xffffffff);
    measure = mulib_glx_text_measure(&builtin, "A\r\nA", 4, 8);
    assert(measure.x == 6 && measure.y == 16);
    measure = mulib_glx_text_measure(&builtin, "A", 1, 9);
    assert(measure.x == 6 && measure.y == 9);
    allocation_budget = 0;
    assert(!mulib_glx_glyphcache_raster(&raster, &builtin, 'A', ~0u));
    allocation_budget = SIZE_MAX;
    assert(!raster.pixels);
    mulib_glx_font_free(&builtin);
    mgx_surface_free(&s);
}

static void
image_rejected(const unsigned char *p, size_t n)
{
    mgx_surface s = {0};
    assert(!mulib_glx_imagedecode(p, n, &s));
    assert(!s.pixels && !s.w && !s.h);
    mgx_surface_free(&s);
}

static size_t
make_bmp(unsigned char *p, int top_down, unsigned bpp)
{
    static const unsigned char pixels24[] = {
        255, 0, 0, 255, 255, 255, 0, 0, /* bottom: blue, white + padding */
        0, 0, 255, 0, 255, 0, 0, 0      /* top: red, green + padding */
    };
    static const unsigned char pixels32[] = {
        255, 0, 0, 0, 255, 255, 255, 128,
        0, 0, 255, 0, 0, 255, 0, 128
    };
    memset(p, 0, 256);
    p[0] = 'B'; p[1] = 'M';
    put32(p + 2, 70);
    put32(p + 10, 54);
    put32(p + 14, 40);
    put32(p + 18, 2);
    put32(p + 22, top_down ? UINT32_MAX - 1 : 2);
    p[26] = 1;
    p[28] = (unsigned char)bpp;
    put32(p + 34, 16);
    memcpy(p + 54, bpp == 24 ? pixels24 : pixels32, 16);
    return 70;
}

static void
test_images(void)
{
    static const unsigned char ppm[] =
        "P6\n# original test pixels\n2 1\n255\n\x0a\x23\x00\xff\x7f\x40";
    static const unsigned char ppm16[] =
        "P6\n1 1\n1023\n\x00\x00\x02\x00\x03\xff";
    static const unsigned char ppm17[] = "P6\n1 1\n17\n\x11\x08\x00";
    static const unsigned char bad_magic[] = "P61 1 255\n\xff\xff\xff";
    static const unsigned char bad_number[] =
        "P6\n42949672960 1\n255\n\xff\xff\xff";
    static const unsigned char bad_sep[] = "P6\n1 1\n255\0\xff\xff\xff";
    static const unsigned char png[] = {137, 'P', 'N', 'G', 13, 10, 26, 10};
    unsigned char bmp[256], bad[256];
    size_t n, at;
    unsigned value;
    mgx_surface s = {0};
    mulib_glx_image_limits_t limits = mulib_glx_image_limits_default();
    for (size_t i = 0; i < sizeof ppm - 1; ++i)
        image_rejected(ppm, i);
    assert(mulib_glx_imagedecode(ppm, sizeof ppm - 1, &s));
    assert(s.w == 2 && s.h == 1 && s.pixels[0] == 0xff0a2300);
    assert(s.pixels[1] == 0xffff7f40);
    assert(!mulib_glx_imagedecode(ppm, sizeof ppm - 1, &s));
    assert(s.pixels[0] == 0xff0a2300);
    mgx_surface_free(&s);
    for (size_t i = 0; i < sizeof ppm16 - 1; ++i)
        image_rejected(ppm16, i);
    assert(mulib_glx_imagedecode(ppm16, sizeof ppm16 - 1, &s));
    assert(s.pixels[0] == 0xff0080ff);
    mgx_surface_free(&s);
    assert(mulib_glx_imagedecode(ppm17, sizeof ppm17 - 1, &s));
    assert(s.pixels[0] == 0xffff7800);
    mgx_surface_free(&s);
    memcpy(bad, ppm16, sizeof ppm16);
    bad[sizeof ppm16 - 3] = 4;                  /* sample 1279 > maxval */
    image_rejected(bad, sizeof ppm16 - 1);
    image_rejected(bad_magic, sizeof bad_magic - 1);
    image_rejected(bad_number, sizeof bad_number - 1);
    image_rejected(bad_sep, sizeof bad_sep - 1);
    image_rejected((const unsigned char *)"P6\n0 1\n255\n", 11);
    image_rejected((const unsigned char *)"P6\n1 0\n255\n", 11);
    image_rejected((const unsigned char *)"P6\n1 1\n0\n", 9);
    image_rejected((const unsigned char *)"P6\n1 1\n65536\n", 13);
    image_rejected(png, sizeof png);
    image_rejected(NULL, 100);
    image_rejected(ppm, limits.max_input_bytes + 1);
    assert(!mulib_glx_imagedecode(ppm, sizeof ppm - 1, NULL));
    at = 2; value = 123;
    assert(!mgx_ppm_token(bad_number, sizeof bad_number - 1, &at, &value));
    assert(at == 2 && value == 123);
    assert(!mgx_ppm_token(NULL, 0, &at, &value));

    limits.max_width = 1;
    assert(!mulib_glx_imagedecode_limited(ppm, sizeof ppm - 1, &s, &limits));
    limits = mulib_glx_image_limits_default(); limits.max_height = 0;
    assert(!mulib_glx_imagedecode_limited(ppm, sizeof ppm - 1, &s, &limits));
    limits = mulib_glx_image_limits_default(); limits.max_pixels = 1;
    assert(!mulib_glx_imagedecode_limited(ppm, sizeof ppm - 1, &s, &limits));
    limits = mulib_glx_image_limits_default(); limits.max_output_bytes = 7;
    assert(!mulib_glx_imagedecode_limited(ppm, sizeof ppm - 1, &s, &limits));
    limits = mulib_glx_image_limits_default();
    limits.max_input_bytes = sizeof ppm - 2;
    assert(!mulib_glx_imagedecode_limited(ppm, sizeof ppm - 1, &s, &limits));
    limits = (mulib_glx_image_limits_t){sizeof ppm - 1, 2, 8, 2, 1};
    assert(mulib_glx_imagedecode_limited(ppm, sizeof ppm - 1, &s, &limits));
    mgx_surface_free(&s);

    for (unsigned bpp = 24; bpp <= 32; bpp += 8) {
        n = make_bmp(bmp, 0, bpp);
        for (size_t i = 0; i < n; ++i)
            image_rejected(bmp, i);
        assert(mulib_glx_imagedecode(bmp, n, &s));
        assert(s.w == 2 && s.h == 2);
        assert(s.pixels[0] == 0xffff0000 && s.pixels[1] == 0xff00ff00);
        assert(s.pixels[2] == 0xff0000ff && s.pixels[3] == 0xffffffff);
        mgx_surface_free(&s);
        n = make_bmp(bmp, 1, bpp);
        assert(mulib_glx_imagedecode(bmp, n, &s));
        assert(s.pixels[0] == 0xff0000ff && s.pixels[2] == 0xffff0000);
        mgx_surface_free(&s);
    }
    n = make_bmp(bmp, 0, 24);
    for (unsigned i = 0; i < 15; ++i) {
        static const unsigned offsets[] = {
            10, 10, 10, 14, 14, 18, 18, 22, 22, 22, 30, 2, 2, 34, 34
        };
        static const uint32_t values[] = {
            0, 53, 71, 41, 108, 0, UINT32_MAX, 0, 0x80000000,
            0x7fffffff, 1, 69, 71, 15, 17
        };
        memcpy(bad, bmp, n);
        put32(bad + offsets[i], values[i]);
        image_rejected(bad, n);
    }
    memcpy(bad, bmp, n); bad[26] = 0;
    image_rejected(bad, n);
    memcpy(bad, bmp, n); bad[28] = 16;
    image_rejected(bad, n);
    memcpy(bad, bmp, n); put32(bad + 2, 0);
    image_rejected(bad, n);
    /* A complete extended DIB must precede its raster. */
    memset(bad, 0, sizeof bad);
    memcpy(bad, bmp, 54); memcpy(bad + 122, bmp + 54, 16);
    put32(bad + 14, 108); put32(bad + 10, 122); put32(bad + 2, 138);
    assert(mulib_glx_imagedecode(bad, 138, &s));
    assert(s.pixels[0] == 0xffff0000);
    mgx_surface_free(&s);
    allocation_budget = 0;
    image_rejected(ppm, sizeof ppm - 1);
    image_rejected(bmp, n);
    allocation_budget = SIZE_MAX;
}

/* Deterministic malformed inputs exercise both parsers and every bitmap byte.
 * No success is fabricated: accepted mutations are rasterized and released. */
static void
test_mutations(void)
{
    unsigned char psf[128], bmp[256], p[256];
    size_t psf_n = make_psf(psf), bmp_n = make_bmp(bmp, 0, 24);
    uint32_t state = 0x13579bdu;
    for (unsigned iteration = 0; iteration < 3000; ++iteration) {
        mulib_glx_font_t f = {0};
        mgx_surface image = {0}, glyph = {0};
        size_t n = iteration & 1 ? psf_n : bmp_n;
        memcpy(p, iteration & 1 ? psf : bmp, n);
        state = state * 1664525u + 1013904223u;
        p[state % n] ^= (unsigned char)(1u << ((state >> 16) % 8));
        if (mulib_glx_font_psf2(&f, p, n)) {
            assert(mulib_glx_glyphcache_raster(&glyph, &f, 0xe9, ~0u));
            mgx_surface_free(&glyph);
            mulib_glx_font_free(&f);
        }
        if (mulib_glx_imagedecode(p, n, &image)) {
            assert(image.w > 0 && image.h > 0 && image.pixels);
            mgx_surface_free(&image);
        }
    }
}

int
main(void)
{
    test_psf();
    test_text_and_atlas();
    test_images();
    test_mutations();
    puts("gfx font/image: Unicode, pixels, malformed inputs and allocation failures OK");
    return 0;
}
