/* Optional direct ABI/reference checks; compiled by tools/check_text.py. */
#define MULIB_IMPL_DYNLOAD
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_UNIPROC
#define MULIB_IMPL_UNIBREAK
#define MULIB_IMPL_BIDI
#define MULIB_IMPL_SYSFONTS
#define MULIB_IMPL_FONTSHAPE
#define MULIB_IMPL_FONTDRAW
#include "mulib-unibreak.h"
#include "mulib-bidi.h"
#include "mulib-sysfonts.h"
#include "mulib-fontshape.h"
#include "mulib-fontdraw.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb.h>
#include <hb-ot.h>
#include <unicode/ubidi.h>
#include <unicode/ubrk.h>
#include <assert.h>

/* Match actual script-covering fonts, not merely a Latin font's .notdef output. */
static void compare_script(mulib_sysfonts_t *fonts, const char *query, const char *text) {
    mulib_sysfonts_font_t match = {0};
    mulib_fontshape_t *shape;
    hb_blob_t *blob;
    hb_face_t *face;
    hb_font_t *font;
    hb_buffer_t *buffer;
    hb_glyph_info_t *info;
    hb_glyph_position_t *positions;
    mulib_fontshape_run_t run = {0};
    unsigned count, j;
    size_t offset = 0, used, n = strlen(text);
    uint32_t cp;
    hb_codepoint_t glyph;
    assert(!mulib_sysfonts_match(fonts, query, &match));
    blob = hb_blob_create_from_file_or_fail(match.path); assert(blob);
    face = hb_face_create(blob, (unsigned)match.index); font = hb_font_create(face);
    hb_ot_font_set_funcs(font); hb_font_set_scale(font, 2048, 2048);
    while (offset < n) {
        assert(mulib_utf8_decode((const unsigned char *)text+offset, n-offset, &cp, &used));
        if (!hb_font_get_nominal_glyph(font, cp, &glyph)) {
            printf("SKIP script font coverage: %s (%s)\n", query, match.family);
            hb_font_destroy(font); hb_face_destroy(face); hb_blob_destroy(blob);
            mulib_sysfonts_font_destroy(&match); return;
        }
        offset += used;
    }
    shape = mulib_fontshape_open(match.path, (unsigned)match.index, 2048, NULL); assert(shape);
    buffer = hb_buffer_create();
    hb_buffer_add_utf8(buffer, text, (int)n, 0, (int)n);
    hb_buffer_set_language(buffer, hb_language_from_string("und", -1));
    hb_buffer_guess_segment_properties(buffer); hb_shape(font, buffer, NULL, 0);
    assert(!mulib_fontshape_shape(shape, text, n, NULL, &run));
    info = hb_buffer_get_glyph_infos(buffer, &count);
    positions = hb_buffer_get_glyph_positions(buffer, NULL);
    assert(count && count == run.count);
    for (j = 0; j < count; ++j) {
        assert(info[j].codepoint && run.glyphs[j].glyph == info[j].codepoint);
        assert(run.glyphs[j].cluster == info[j].cluster);
        assert(run.glyphs[j].x_advance == positions[j].x_advance && run.glyphs[j].y_advance == positions[j].y_advance);
        assert(run.glyphs[j].x_offset == positions[j].x_offset && run.glyphs[j].y_offset == positions[j].y_offset);
    }
    mulib_fontshape_run_destroy(&run); mulib_fontshape_close(shape); hb_buffer_destroy(buffer);
    hb_font_destroy(font); hb_face_destroy(face); hb_blob_destroy(blob); mulib_sysfonts_font_destroy(&match);
}

int main(void) {
    mulib_sysfonts_t *fonts = mulib_sysfonts_open(NULL);
    mulib_sysfonts_font_t match = {0};
    mulib_fontdraw_t *draw;
    mulib_fontshape_t *shape;
    FT_Library library;
    FT_Face face;
    hb_blob_t *blob;
    hb_face_t *hb_face;
    hb_font_t *font;
    size_t i;
    const char *texts[] = {"office", "A\xcc\x81", "\xd8\xb3\xd9\x84\xd8\xa7\xd9\x85", "\xe0\xa4\x95\xe0\xa5\x8d\xe0\xa4\xb7"};
    assert(sizeof(mulib_fontdraw_native_bitmap_) == sizeof(FT_Bitmap));
    assert(sizeof(mulib_fontdraw_native_outline_) == sizeof(FT_Outline));
    assert(offsetof(mulib_fontdraw_face_prefix_, glyph) == offsetof(FT_FaceRec, glyph));
    assert(offsetof(mulib_fontdraw_slot_prefix_, outline) == offsetof(FT_GlyphSlotRec, outline));
    assert(offsetof(mulib_fontdraw_slot_prefix_, bitmap) == offsetof(FT_GlyphSlotRec, bitmap));
    assert(sizeof(mulib_fontshape_info_) == sizeof(hb_glyph_info_t));
    assert(sizeof(mulib_fontshape_pos_) == sizeof(hb_glyph_position_t));
    assert(offsetof(mulib_fontshape_info_, cluster) == offsetof(hb_glyph_info_t, cluster));
    assert(offsetof(mulib_fontshape_pos_, xo) == offsetof(hb_glyph_position_t, x_offset));
    assert(fonts && !mulib_sysfonts_match(fonts, "sans", &match));
    draw = mulib_fontdraw_open(match.path, match.index, 32, NULL);
    shape = mulib_fontshape_open(match.path, (unsigned)match.index, 2048, NULL);
    assert(draw && shape && !FT_Init_FreeType(&library) && !FT_New_Face(library, match.path, match.index, &face));
    assert(!FT_Set_Pixel_Sizes(face, 0, 32));
    for (i = 33; i < 127; ++i) {
        FT_UInt glyph = FT_Get_Char_Index(face, (FT_ULong)i);
        mulib_fontdraw_bitmap_t b = {0};
        mulib_fontdraw_outline_t o = {0};
        size_t y, p;
        assert(glyph == mulib_fontdraw_glyph(draw, (uint32_t)i));
        assert(!FT_Load_Glyph(face, glyph, FT_LOAD_DEFAULT) && !FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL));
        assert(!mulib_fontdraw_render(draw, glyph, 0, 0, &b));
        assert(b.width == face->glyph->bitmap.width && b.height == face->glyph->bitmap.rows);
        assert(b.left == face->glyph->bitmap_left && b.top == face->glyph->bitmap_top && b.advance_x == face->glyph->advance.x);
        for (y = 0; y < b.height; ++y) assert(!memcmp(b.pixels+y*b.stride, face->glyph->bitmap.buffer+y*(size_t)face->glyph->bitmap.pitch, b.width));
        mulib_fontdraw_bitmap_destroy(&b);
        assert(!FT_Load_Glyph(face, glyph, FT_LOAD_NO_BITMAP));
        assert(!mulib_fontdraw_outline(draw, glyph, 0, &o));
        assert(o.point_count == (size_t)face->glyph->outline.n_points && o.contour_count == (size_t)face->glyph->outline.n_contours);
        for (p = 0; p < o.point_count; ++p) {
            assert(o.points[p].x == face->glyph->outline.points[p].x && o.points[p].y == face->glyph->outline.points[p].y);
            assert(o.points[p].tag == (unsigned char)face->glyph->outline.tags[p]);
        }
        mulib_fontdraw_outline_destroy(&o);
    }
    blob = hb_blob_create_from_file_or_fail(match.path); assert(blob);
    hb_face = hb_face_create(blob, (unsigned)match.index); font = hb_font_create(hb_face);
    hb_ot_font_set_funcs(font); hb_font_set_scale(font, 2048, 2048);
    for (i = 0; i < sizeof texts/sizeof *texts; ++i) {
        hb_buffer_t *buffer = hb_buffer_create();
        mulib_fontshape_run_t r = {0};
        hb_glyph_info_t *info;
        hb_glyph_position_t *pos;
        unsigned count, j;
        hb_buffer_add_utf8(buffer, texts[i], -1, 0, -1);
        hb_buffer_set_language(buffer, hb_language_from_string("und", -1));
        hb_buffer_guess_segment_properties(buffer); hb_shape(font, buffer, NULL, 0);
        assert(!mulib_fontshape_shape(shape, texts[i], strlen(texts[i]), NULL, &r));
        info = hb_buffer_get_glyph_infos(buffer, &count); pos = hb_buffer_get_glyph_positions(buffer, NULL);
        assert(r.count == count);
        for (j = 0; j < count; ++j) {
            assert(r.glyphs[j].glyph == info[j].codepoint && r.glyphs[j].cluster == info[j].cluster);
            assert(r.glyphs[j].x_advance == pos[j].x_advance && r.glyphs[j].y_advance == pos[j].y_advance);
            assert(r.glyphs[j].x_offset == pos[j].x_offset && r.glyphs[j].y_offset == pos[j].y_offset);
        }
        mulib_fontshape_run_destroy(&r); hb_buffer_destroy(buffer);
    }
    {
        const char text[] = "abc \xe2\x81\xa7\xd7\x90\xd7\x91 12\xe2\x81\xa9 xyz";
        mulib_uniproc_t *u = mulib_uniproc_open(NULL, 0);
        mulib_bidi_result_t result = {0};
        uint16_t *utf16;
        size_t units, *map;
        UErrorCode error = U_ZERO_ERROR;
        UBiDi *bidi = ubidi_open();
        int32_t nr, j;
        assert(u && bidi && !mulib_uniproc_utf16(text, sizeof text-1, &utf16, &units, &map));
        ubidi_setPara(bidi, utf16, (int32_t)units, UBIDI_DEFAULT_LTR, NULL, &error); assert(U_SUCCESS(error));
        nr = ubidi_countRuns(bidi, &error); assert(U_SUCCESS(error));
        assert(!mulib_bidi_resolve(u, text, sizeof text-1, -1, &result) && result.count == (size_t)nr);
        for (j = 0; j < nr; ++j) {
            int32_t start, length;
            (void)ubidi_getVisualRun(bidi, j, &start, &length);
            assert(result.runs[j].start == map[start] && result.runs[j].length == map[start+length]-map[start]);
            assert(result.runs[j].level == ubidi_getLevelAt(bidi, start));
        }
        for (j = 0; j < 4; ++j) {
            UBreakIterator *it;
            mulib_unibreak_boundary_t *boundaries;
            size_t count, k = 0;
            int32_t at;
            error = U_ZERO_ERROR;
            it = ubrk_open((UBreakIteratorType)j, "", utf16, (int32_t)units, &error); assert(it && U_SUCCESS(error));
            assert(!mulib_unibreak_find(u, (mulib_unibreak_kind_t)j, NULL, text, sizeof text-1, &boundaries, &count));
            for (at = ubrk_first(it); at != UBRK_DONE; at = ubrk_next(it)) {
                assert(k < count && boundaries[k].offset == map[at] && boundaries[k].status == ubrk_getRuleStatus(it)); ++k;
            }
            assert(k == count); free(boundaries); ubrk_close(it);
        }
        ubidi_close(bidi); mulib_bidi_destroy(&result); free(utf16); free(map); mulib_uniproc_close(u);
    }
    hb_font_destroy(font); hb_face_destroy(hb_face); hb_blob_destroy(blob);
    FT_Done_Face(face); FT_Done_FreeType(library);
    mulib_fontdraw_close(draw); mulib_fontshape_close(shape);
    mulib_sysfonts_font_destroy(&match);
    compare_script(fonts, "sans:lang=ar", "\xd8\xb3\xd9\x84\xd8\xa7\xd9\x85");
    compare_script(fonts, "sans:lang=hi", "\xe0\xa4\x95\xe0\xa5\x8d\xe0\xa4\xb7");
    mulib_sysfonts_close(fonts);
    return 0;
}
