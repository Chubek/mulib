#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static size_t remaining = SIZE_MAX, live;
static int allocation_allowed(void) {
    if (remaining == SIZE_MAX) return 1;
    if (!remaining) return 0;
    --remaining; return 1;
}
static void *checked_malloc(size_t n) {
    void *p;
    if (!allocation_allowed()) return NULL;
    p = malloc(n); if (p) ++live; return p;
}
static void *checked_calloc(size_t n, size_t stride) {
    void *p;
    if (!allocation_allowed()) return NULL;
    p = calloc(n, stride); if (p) ++live; return p;
}
static void *checked_realloc(void *p, size_t n) {
    void *q;
    int was_null = p == NULL;
    if (!n) { if (p) { assert(live); --live; } free(p); return NULL; }
    if (!allocation_allowed()) return NULL;
    q = realloc(p, n); if (q && was_null) ++live; return q;
}
static void checked_free(void *p) { if (p) { assert(live); --live; } free(p); }
#define malloc checked_malloc
#define calloc checked_calloc
#define realloc checked_realloc
#define free checked_free
#define MULIB_IMPL_DYNLOAD
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_JSON
#define MULIB_IMPL_YAML
#define MULIB_IMPL_XML
#define MULIB_IMPL_CSS
#define MULIB_IMPL_STYLESHEET
#define MULIB_IMPL_UNIPROC
#define MULIB_IMPL_UNIBREAK
#define MULIB_IMPL_UNINAME
#define MULIB_IMPL_BIDI
#define MULIB_IMPL_FONTSHAPE
#define MULIB_IMPL_FONTDRAW
#define MULIB_IMPL_SYSFONTS
#define MULIB_IMPL_DISTFIELD
#define MULIB_IMPL_PGLAYOUT
#include "mulib-pglayout.h"
#include "mulib-uniname.h"
#include "mulib-fontdraw.h"
#include "mulib-sysfonts.h"
#include "mulib-distfield.h"

static int measure_fixed(void *ctx, const char *s, size_t n, const mulib_stylesheet_style_t *style, double *width) {
    (void)ctx; (void)s; (void)style; *width = (double)n*8; return 0;
}
int main(void) {
    size_t limit, baseline;
    int success;
    mulib_uniproc_t *u;
    mulib_pglayout_item_t *root, *child;
    mulib_stylesheet_t *sheet;
    const char css[] = "text {font-size:12px;} .a {padding:2px;}";
    for (limit = 0, success = 0; limit < 200 && !success; ++limit) {
        remaining = limit;
        sheet = mulib_stylesheet_parse(css, sizeof css-1, NULL);
        success = sheet != NULL; mulib_stylesheet_destroy(sheet); assert(live == 0);
    }
    assert(success);
    for (limit = 0, success = 0; limit < 20 && !success; ++limit) {
        unsigned char mask[] = {0,255,0,255}, *field = NULL;
        remaining = limit;
        success = !mulib_distfield_generate(mask, 2, 2, 2, 2, &field);
        free(field); assert(live == 0);
    }
    assert(success);
    remaining = SIZE_MAX;
    root = mulib_pglayout_new(MULIB_PGLAYOUT_LAYOUT); child = mulib_pglayout_new(MULIB_PGLAYOUT_TEXT);
    assert(root && child && !mulib_pglayout_content(child, "one two three\n"));
    assert(!mulib_pglayout_attr(child, "class", "a") && !mulib_pglayout_append(root, child));
    baseline = live;
    for (limit = 0, success = 0; limit < 300 && !success; ++limit) {
        char *s;
        remaining = limit; s = mulib_pglayout_serialize(root, 1);
        success = s != NULL; free(s); assert(live == baseline);
    }
    assert(success);
    remaining = SIZE_MAX;
    {
        char *s = mulib_pglayout_serialize(root, 0);
        size_t saved = live;
        assert(s);
        for (limit = 0, success = 0; limit < 500 && !success; ++limit) {
            mulib_pglayout_item_t *copy;
            remaining = limit; copy = mulib_pglayout_deserialize(s, strlen(s), 0);
            success = copy != NULL; mulib_pglayout_destroy(copy); assert(live == saved);
        }
        assert(success); free(s);
    }
    remaining = SIZE_MAX; u = mulib_uniproc_open(NULL, 0);
    if (u) {
        baseline = live;
        for (limit = 0, success = 0; limit < 50 && !success; ++limit) {
            char *s;
            remaining = limit; s = mulib_uniproc_normalize(u, MULIB_UNIPROC_NFC, "e\xcc\x81", 3, NULL);
            success = s != NULL; free(s); assert(live == baseline);
        }
        assert(success);
        for (limit = 0, success = 0; limit < 50 && !success; ++limit) {
            mulib_bidi_result_t bidi = {0};
            remaining = limit; success = !mulib_bidi_resolve(u, "abc \xd7\x90", 6, -1, &bidi);
            mulib_bidi_destroy(&bidi); assert(live == baseline);
        }
        assert(success);
        for (limit = 0, success = 0; limit < 50 && !success; ++limit) {
            char *s;
            size_t length = 777;
            remaining = limit;
            s = mulib_uniproc_case_map(u, MULIB_UNIPROC_UPPER, NULL, "Stra\xc3\x9f" "e", 7, &length);
            success = s != NULL;
            if (!success) assert(length == 777);
            free(s); assert(live == baseline);
        }
        assert(success);
        for (limit = 0, success = 0; limit < 50 && !success; ++limit) {
            mulib_unibreak_boundary_t *b = NULL;
            size_t count = 777;
            remaining = limit;
            success = !mulib_unibreak_find(u, MULIB_UNIBREAK_GRAPHEME, NULL, "e\xcc\x81", 3, &b, &count);
            if (!success) assert(!b && count == 777);
            free(b); assert(live == baseline);
        }
        assert(success);
        for (limit = 0, success = 0; limit < 10 && !success; ++limit) {
            char *name;
            remaining = limit; name = mulib_uniname_name(u, 'A', 0);
            success = name != NULL; free(name); assert(live == baseline);
        }
        assert(success);
        for (limit = 0, success = 0; limit < 100 && !success; ++limit) {
            remaining = limit;
            success = !mulib_pglayout_compute(root, NULL, u, 40, measure_fixed, NULL);
            if (!success) assert(live == baseline);
        }
        assert(success);
        /* Failed recomputation must retain every previous geometry/line pointer. */
        {
            mulib_pglayout_box_t before, after;
            const mulib_pglayout_line_t *lines;
            size_t count;
            baseline = live;
            assert(!mulib_pglayout_geometry(root, &before, NULL));
            lines = mulib_pglayout_lines(child, &count); assert(lines && count);
            for (limit = 0, success = 0; limit < 100 && !success; ++limit) {
                remaining = limit;
                success = !mulib_pglayout_compute(root, NULL, u, 80, measure_fixed, NULL);
                if (!success) {
                    size_t after_count;
                    assert(live == baseline && !mulib_pglayout_geometry(root, &after, NULL));
                    assert(before.height == after.height && before.width == after.width);
                    assert(mulib_pglayout_lines(child, &after_count) == lines && after_count == count);
                }
            }
            assert(success);
        }
        mulib_uniproc_close(u);
    } else assert(!getenv("MULIB_TEXT_REQUIRE_BACKENDS"));
    mulib_pglayout_destroy(root); assert(live == 0);
    remaining = SIZE_MAX;
    {
        mulib_sysfonts_t *fonts = mulib_sysfonts_open(NULL);
        if (fonts) {
            mulib_sysfonts_font_t record = {0};
            mulib_fontshape_t *shape;
            mulib_fontdraw_t *draw;
            baseline = live;
            for (limit = 0, success = 0; limit < 20 && !success; ++limit) {
                remaining = limit; success = !mulib_sysfonts_match(fonts, "sans", &record);
                if (!success) assert(live == baseline);
            }
            assert(success);
            {
                mulib_sysfonts_font_t *records = NULL;
                size_t count, i;
                for (limit = 0, success = 0; limit < 1000 && !success; ++limit) {
                    count = 777;
                    remaining = limit;
                    success = !mulib_sysfonts_list(fonts, record.family, &records, &count);
                    if (!success) assert(!records && count == 777);
                    else {
                        for (i = 0; i < count; ++i) mulib_sysfonts_font_destroy(&records[i]);
                        free(records); records = NULL;
                    }
                    assert(live == baseline + 3); /* match owns three strings */
                }
                assert(success);
            }
            remaining = SIZE_MAX;
            shape = mulib_fontshape_open(record.path, (unsigned)record.index, 1024, NULL);
            draw = mulib_fontdraw_open(record.path, record.index, 16, NULL);
            baseline = live;
            if (shape) {
                for (limit = 0, success = 0; limit < 10 && !success; ++limit) {
                    mulib_fontshape_run_t run = {0};
                    remaining = limit; success = !mulib_fontshape_shape(shape, "office", 6, NULL, &run);
                    mulib_fontshape_run_destroy(&run); assert(live == baseline);
                }
                assert(success);
            } else assert(!getenv("MULIB_TEXT_REQUIRE_BACKENDS"));
            if (draw) {
                for (limit = 0, success = 0; limit < 10 && !success; ++limit) {
                    mulib_fontdraw_bitmap_t bitmap = {0};
                    unsigned char *alpha = NULL;
                    remaining = limit;
                    success = !mulib_fontdraw_render(draw, mulib_fontdraw_glyph(draw, 'A'), 0, 0, &bitmap);
                    if (success) success = !mulib_fontdraw_alpha(&bitmap, &alpha);
                    free(alpha); mulib_fontdraw_bitmap_destroy(&bitmap); assert(live == baseline);
                }
                assert(success);
                for (limit = 0, success = 0; limit < 10 && !success; ++limit) {
                    mulib_fontdraw_outline_t outline = {0};
                    remaining = limit; success = !mulib_fontdraw_outline(draw, mulib_fontdraw_glyph(draw, 'A'), 0, &outline);
                    mulib_fontdraw_outline_destroy(&outline); assert(live == baseline);
                }
                assert(success);
            } else assert(!getenv("MULIB_TEXT_REQUIRE_BACKENDS"));
            mulib_fontshape_close(shape); mulib_fontdraw_close(draw);
            mulib_sysfonts_font_destroy(&record); mulib_sysfonts_close(fonts); assert(live == 0);
        } else assert(!getenv("MULIB_TEXT_REQUIRE_BACKENDS"));
    }
    return 0;
}
