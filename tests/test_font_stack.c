#define MULIB_IMPL_DYNLOAD
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_SYSFONTS
#define MULIB_IMPL_FONTSHAPE
#define MULIB_IMPL_FONTDRAW
#include "mulib-sysfonts.h"
#include "mulib-fontshape.h"
#include "mulib-fontdraw.h"
#include <assert.h>

int main(void) {
    mulib_sysfonts_t *fonts;
    mulib_sysfonts_font_t match = {0}, *list = NULL;
    mulib_fontshape_t *shape;
    mulib_fontdraw_t *draw;
    mulib_fontshape_run_t run = {0}, plain = {0};
    mulib_fontdraw_bitmap_t bitmap = {0};
    mulib_fontdraw_outline_t outline = {0};
    size_t count, i, ink = 0;
    unsigned char *alpha = NULL;
    unsigned glyph;
    const char *features[] = {"liga=0", "clig=0"};
    mulib_fontshape_options_t options = {"ltr", "Latn", "en", features, 2};
    {
        unsigned char packed[] = {0x1b, 0xe4};
        unsigned char bgra[] = {0,0,0,0, 10,20,30,128};
        mulib_fontdraw_bitmap_t b = {packed, 4, 2, 1, 3, 4, 0, 0, 0, 0};
        assert(!mulib_fontdraw_alpha(&b, &alpha));
        assert(alpha[0] == 0 && alpha[1] == 85 && alpha[2] == 170 && alpha[3] == 255);
        assert(alpha[4] == 255 && alpha[5] == 170 && alpha[6] == 85 && alpha[7] == 0); free(alpha);
        b.pixels = bgra; b.width = 2; b.height = 1; b.stride = 8; b.pixel_mode = 7;
        assert(!mulib_fontdraw_alpha(&b, &alpha) && alpha[0] == 0 && alpha[1] == 128); free(alpha); alpha = NULL;
        b.pixel_mode = 5; assert(mulib_fontdraw_alpha(&b, &alpha) == -1 && !alpha);
        b.pixel_mode = 7; b.stride = 1; assert(mulib_fontdraw_alpha(&b, &alpha) == -1);
    }
    assert(!mulib_sysfonts_open("/nonexistent/fontconfig"));
    assert(!mulib_fontshape_open("/nonexistent/font", 0, 1024, NULL));
    assert(!mulib_fontdraw_open("/nonexistent/font", 0, 16, NULL));
    fonts = mulib_sysfonts_open(NULL);
    if (!fonts) { assert(!getenv("MULIB_TEXT_REQUIRE_BACKENDS")); puts("SKIP: Fontconfig unavailable"); return 0; }
    assert(!mulib_sysfonts_match(fonts, "sans", &match));
    assert(match.path && *match.path && match.family && match.style);
    assert(!mulib_sysfonts_list(fonts, NULL, &list, &count) && count);
    for (i = 0; i < count; ++i) { assert(list[i].path); mulib_sysfonts_font_destroy(&list[i]); }
    free(list);
    assert(mulib_sysfonts_add(fonts, "/nonexistent/font") == -1);
    assert(!mulib_sysfonts_add(fonts, match.path)); mulib_sysfonts_clear(fonts);
    shape = mulib_fontshape_open(match.path, (unsigned)match.index, 32*64, NULL);
    draw = mulib_fontdraw_open(match.path, match.index, 32, NULL);
    if (!shape || !draw) {
        assert(!getenv("MULIB_TEXT_REQUIRE_BACKENDS")); puts("SKIP: HarfBuzz/FreeType unavailable");
        mulib_fontshape_close(shape); mulib_fontdraw_close(draw);
        mulib_sysfonts_font_destroy(&match); mulib_sysfonts_close(fonts); return 0;
    }
    /* Returned match records own strings independently of the native context. */
    mulib_sysfonts_close(fonts);
    assert(!mulib_fontshape_open(match.path, 65535, 2048, NULL));
    assert(!mulib_fontshape_shape(shape, "office", 6, NULL, &run) && run.count > 0);
    assert(!mulib_fontshape_shape(shape, "office", 6, &options, &plain));
    assert(run.count <= plain.count && plain.count == 6);
    for (i = 0; i < run.count; ++i) assert(run.glyphs[i].cluster < 6 && run.glyphs[i].glyph);
    mulib_fontshape_run_destroy(&run); mulib_fontshape_run_destroy(&plain);
    assert(!mulib_fontshape_shape(shape, NULL, 0, NULL, &run) && run.count == 0);
    mulib_fontshape_run_destroy(&run);
    assert(mulib_fontshape_shape(shape, "\xc0\x80", 2, NULL, &run) == -1);
    options.direction = "invalid";
    assert(mulib_fontshape_shape(shape, "a", 1, &options, &run) == -1);
    options.direction = "ltr"; options.script = "1234";
    assert(mulib_fontshape_shape(shape, "a", 1, &options, &run) == -1);
    options.direction = "rtl"; options.script = "Arab"; options.language = "ar"; options.feature_count = 0;
    assert(!mulib_fontshape_shape(shape, "\xd8\xb3\xd9\x84\xd8\xa7\xd9\x85", 8, &options, &run));
    assert(run.count > 0 && run.glyphs[0].cluster >= run.glyphs[run.count-1].cluster);
    mulib_fontshape_run_destroy(&run);
    assert(!mulib_fontshape_variations(shape, NULL, 0));
    { const char *bad[] = {"not an axis"}; assert(mulib_fontshape_variations(shape, bad, 1) == -1); }
    { const char *bad[] = {"wght=nan"}; assert(mulib_fontshape_variations(shape, bad, 1) == -1); }
    glyph = mulib_fontdraw_glyph(draw, 'A'); assert(glyph);
    assert(!mulib_fontdraw_render(draw, glyph, 0, 0, &bitmap));
    assert(bitmap.width && bitmap.height && bitmap.pixel_mode == 2 && bitmap.advance_x > 0);
    assert(!mulib_fontdraw_alpha(&bitmap, &alpha));
    for (i = 0; i < bitmap.width*bitmap.height; ++i) ink += alpha[i];
    assert(ink > 0); free(alpha); mulib_fontdraw_bitmap_destroy(&bitmap);
    assert(!mulib_fontdraw_render(draw, glyph, 0, 2, &bitmap));
    assert(bitmap.pixel_mode == 1 && !mulib_fontdraw_alpha(&bitmap, &alpha));
    free(alpha); mulib_fontdraw_bitmap_destroy(&bitmap);
    assert(!mulib_fontdraw_outline(draw, glyph, 0, &outline));
    assert(outline.point_count && outline.contour_count && outline.contours[outline.contour_count-1] == outline.point_count-1);
    mulib_fontdraw_outline_destroy(&outline);
    assert(mulib_fontdraw_render(draw, UINT_MAX, 0, 0, &bitmap) == -1);
    assert(!mulib_fontdraw_size(draw, 16) && mulib_fontdraw_size(draw, 0) == -1);
    mulib_fontshape_close(shape); mulib_fontdraw_close(draw); mulib_sysfonts_font_destroy(&match);
    return 0;
}
