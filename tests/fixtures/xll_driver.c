#include "xll_implementation.h"
#include <assert.h>
#include "xll-generated.c"
#if !defined(MULIB_XLL_NO_REFERENCE) && !defined(MULIB_XLL_EMPTY) && !defined(MULIB_XLL_LONG)
#include "xll_source.h"
static void compare_results(const mulib_layout_result_t *a, const mulib_layout_result_t *b) {
    size_t i, j;
    assert(a->length == b->length && !memcmp(a->text, b->text, a->length));
    assert(a->count == b->count && a->width == b->width && a->height == b->height);
    for (i = 0; i < a->count; ++i) {
        const mulib_layout_line_t *x = &a->lines[i], *y = &b->lines[i];
        assert(x->start == y->start && x->length == y->length && x->next == y->next);
        assert(x->box.x == y->box.x && x->box.y == y->box.y && x->box.width == y->box.width && x->box.height == y->box.height);
        assert(x->baseline == y->baseline && x->ascent == y->ascent && x->descent == y->descent);
        assert(x->run.count == y->run.count && x->run.paragraph_level == y->run.paragraph_level);
        for (j = 0; j < x->run.count; ++j) {
            const mulib_glyph_t *g = &x->run.glyphs[j], *h = &y->run.glyphs[j];
            assert(g->id == h->id && g->start == h->start && g->length == h->length && g->font_index == h->font_index);
            assert(g->level == h->level && g->x == h->x && g->y == h->y && g->advance == h->advance);
        }
    }
}
#endif
int main(void) {
#if defined(MULIB_XLL_EMPTY)
    assert(!mulib_xll_empty_count() && !mulib_xll_empty_get(0));
    assert(!mulib_xll_empty_create(0, NULL, NULL));
#elif defined(MULIB_XLL_LONG)
    const mulib_xll_longtext_block_t *b = mulib_xll_longtext_get(3);
    assert(mulib_xll_longtext_count() == 4 && b && b->length == 8192 && b->text[4000] == 0);
    assert(b->text[3999] == 'x' && b->text[8191] == 'x' && !b->text[8192]);
#else
    mulib_sysfonts_t *catalog;
    mulib_uniproc_t *unicode;
    mulib_glyph_font_t *paired;
    mulib_fontshape_t *shape;
    mulib_pglayout_fontset_t fonts;
    mulib_layout_result_t actual = {0};
    mulib_xml_error_t error;
    size_t i;
#ifndef MULIB_XLL_NO_REFERENCE
    mulib_xll_t *program = mulib_xll_parse(xll_source, strlen(xll_source), NULL);
    assert(program);
    for (i = 0; i < mulib_xll_count(program); ++i) {
        const mulib_xll_block_t *a = mulib_xll_get(program, i);
        const mulib_xll_fixture_block_t *b = mulib_xll_fixture_get(i);
        assert(b && !strcmp(a->name, b->name) && !strcmp(a->font_family, b->font_family) && !strcmp(a->language, b->language));
        assert(a->length == b->length && !memcmp(a->text, b->text, a->length));
        assert(a->font_size == b->font_size && a->x == b->x && a->y == b->y);
        assert(!!signbit(a->x) == !!signbit(b->x) && !!signbit(a->y) == !!signbit(b->y));
        assert(a->options.width == b->options.width && a->options.spacing == b->options.spacing &&
            a->options.wrap == b->options.wrap && a->options.alignment == b->options.alignment && a->options.base == b->options.base);
    }
#endif
    assert(mulib_xll_fixture_count() == 3 && !mulib_xll_fixture_get(SIZE_MAX));
    assert(!strcmp(mulib_xll_fixture_get(0)->text, "Hello سلام world"));
    assert(mulib_xll_fixture_get(1)->text[1] == 0 && mulib_xll_fixture_get(1)->length > 2);
    assert(!mulib_xll_fixture_create(0, NULL, &error) && error.message);
    catalog = mulib_sysfonts_open(NULL); unicode = mulib_uniproc_open(NULL, 0);
    paired = catalog ? mulib_glyph_font_match(catalog, "DejaVu Sans", 18) : NULL;
    if (!paired || !unicode) {
        assert(!getenv("MULIB_TEXT_REQUIRE_BACKENDS")); puts("SKIP: native XLL rendering backends unavailable");
    } else {
        shape = mulib_glyph_font_shape(paired);
        fonts.unicode = unicode; fonts.fonts = &shape; fonts.count = 1; fonts.language = "en"; fonts.base = 0;
        for (i = 0; i < 3; i += 2) {
            assert(!mulib_xll_fixture_compute(i, &fonts, &actual, &error));
#ifndef MULIB_XLL_NO_REFERENCE
            {
                mulib_layout_result_t reference = {0};
                assert(!mulib_xll_compute(program, i, &fonts, &reference, NULL));
                compare_results(&actual, &reference); mulib_layout_result_destroy(&reference);
            }
#endif
            assert(actual.count && actual.height > 0);
            assert(mulib_xll_fixture_compute(i, &fonts, &actual, &error) == -1 && actual.count);
            mulib_layout_result_destroy(&actual);
        }
        {
            mulib_sysfonts_font_t record = {0}; mulib_fontshape_t *wrong;
            assert(!mulib_sysfonts_match(catalog, "sans", &record));
            wrong = mulib_fontshape_open(record.path, (unsigned)record.index, 16*64, NULL); assert(wrong);
            fonts.fonts = &wrong;
            assert(!mulib_xll_fixture_create(0, &fonts, &error) && error.message);
            mulib_fontshape_close(wrong); mulib_sysfonts_font_destroy(&record);
        }
    }
    mulib_glyph_font_close(paired); mulib_uniproc_close(unicode); mulib_sysfonts_close(catalog);
#ifndef MULIB_XLL_NO_REFERENCE
    mulib_xll_destroy(program);
#endif
#endif
    return 0;
}
