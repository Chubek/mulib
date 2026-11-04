#include <stdlib.h>
static size_t allocation, fail_at = (size_t)-1;
static void *layout_malloc(size_t n) { return allocation++ == fail_at ? NULL : malloc(n); }
static void *layout_calloc(size_t n, size_t s) { return allocation++ == fail_at ? NULL : calloc(n, s); }
static void *layout_realloc(void *p, size_t n) { return allocation++ == fail_at ? NULL : realloc(p, n); }
#define malloc layout_malloc
#define calloc layout_calloc
#define realloc layout_realloc
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
#define MULIB_IMPL_BIDI
#define MULIB_IMPL_FONTSHAPE
#define MULIB_IMPL_SYSFONTS
#define MULIB_IMPL_FONTDRAW
#define MULIB_IMPL_PGLAYOUT
#define MULIB_IMPL_LIGATURE
#define MULIB_IMPL_GLYPH
#define MULIB_IMPL_LAYOUT
#include "mulib-layout.h"
#undef malloc
#undef calloc
#undef realloc
#include <assert.h>

typedef struct draw_check { const mulib_layout_result_t *result; size_t line, glyph, count; } draw_check;
static int draw(void *ctx, const mulib_glyph_t *g) {
    draw_check *c = (draw_check *)ctx;
    const mulib_glyph_t *original;
    while (c->line < c->result->count && c->glyph == c->result->lines[c->line].run.count) { ++c->line; c->glyph = 0; }
    assert(c->line < c->result->count);
    original = &c->result->lines[c->line].run.glyphs[c->glyph++];
    assert(g->id == original->id && g->start == original->start);
    assert(g->x == original->x+20 && g->y == original->y+40); ++c->count; return 0;
}
static int stop_draw(void *ctx, const mulib_glyph_t *g) { (void)ctx; (void)g; return 7; }
int main(void) {
    mulib_sysfonts_t *catalog = mulib_sysfonts_open(NULL);
    mulib_uniproc_t *u = mulib_uniproc_open(NULL, 0);
    mulib_glyph_font_t *paired = catalog ? mulib_glyph_font_match(catalog, "DejaVu Sans", 18) : NULL;
    mulib_fontshape_t *font = mulib_glyph_font_shape(paired);
    mulib_pglayout_fontset_t fonts = {u, &font, 1, "en", -1};
    mulib_layout_t *l;
    mulib_layout_options_t o = mulib_layout_default();
    mulib_layout_result_t r = {0};
    mulib_xml_error_t error;
    size_t i, j, hit, count;
    const char *mixed = "Hello \xd8\xb3\xd9\x84\xd8\xa7\xd9\x85 world";
    {
        mulib_fontshape_glyph_t gs[] = {{1, 0, 1, 0, 0, 0}, {2, 1, 1, 0, 0, 0}};
        mulib_fontshape_run_t run = {gs, 2};
        mulib_ligature_cluster_t *c = NULL;
        assert(!mulib_ligature_clusters("ffi", 3, &run, &c, &count));
        assert(count == 2 && c[1].length == 2 && c[1].scalar_count == 2); free(c); c = NULL;
        gs[0].cluster = 1; gs[1].cluster = 0;
        assert(!mulib_ligature_clusters("ffi", 3, &run, &c, &count) && c[0].length == 2); free(c); c = NULL;
        assert(mulib_ligature_clusters("\xc3\xa9", 2, &run, &c, &count) == -1 && !c);
        assert(!mulib_layout_create(NULL));
        assert(mulib_layout_compute(NULL, &r, &error) == -1 && error.message && !r.lines);
    }
    if (!paired || !u) {
        assert(!getenv("MULIB_TEXT_REQUIRE_BACKENDS")); puts("SKIP: text layout backends unavailable");
        mulib_glyph_font_close(paired); mulib_uniproc_close(u); mulib_sysfonts_close(catalog); return 0;
    }
    l = mulib_layout_create(&fonts); assert(l);
    assert(!mulib_layout_compute(l, &r, &error) && r.count == 1 && !r.length && r.height > 0);
    mulib_layout_result_destroy(&r);
    {
        size_t total;
        assert(!mulib_layout_set_text(l, "office", 6));
        allocation = 0;
        assert(!mulib_layout_compute(l, &r, NULL)); total = allocation;
        mulib_layout_result_destroy(&r);
        for (i = 0; i < total; ++i) {
            allocation = 0; fail_at = i;
            assert(mulib_layout_compute(l, &r, &error) == -1 && error.message && !r.lines && !r.text);
        }
        allocation = 0; fail_at = 0;
        assert(mulib_layout_set_text(l, "changed", 7) == -1);
        fail_at = (size_t)-1;
        assert(!mulib_layout_compute(l, &r, NULL) && !strcmp(r.text, "office"));
        mulib_layout_result_destroy(&r);
    }
    assert(!mulib_layout_set_text(l, mixed, strlen(mixed)));
    assert(mulib_layout_set_text(l, "\xc0\x80", 2) == -1);
    o.width = 400; o.wrap = MULIB_LAYOUT_WRAP_WORD;
    assert(!mulib_layout_configure(l, &o) && !mulib_layout_compute(l, &r, &error));
    assert(r.count == 1 && !strcmp(r.text, mixed) && r.lines[0].run.count > 0 && r.lines[0].ascent > 0);
    {
        int rtl = 0; draw_check c = {&r, 0, 0, 0};
        for (i = 0; i < r.lines[0].run.count; ++i) {
            mulib_glyph_t *g = &r.lines[0].run.glyphs[i];
            assert(g->start+g->length <= strlen(mixed) && g->font_index == 0);
            if (g->level & 1) rtl = 1;
        }
        assert(rtl && !mulib_layout_draw(&r, 20, 40, draw, &c) && c.count);
        assert(mulib_layout_draw(&r, 0, 0, stop_draw, NULL) == 7);
        assert(!mulib_layout_hit_test(&r, -100, 0, &hit) && hit == 0);
        assert(!mulib_layout_hit_test(&r, 1000, 0, &hit) && hit == strlen(mixed));
    }
    assert(mulib_layout_compute(l, &r, &error) == -1 && r.count == 1);
    mulib_layout_result_destroy(&r);
    assert(!mulib_layout_set_text(l, "\xd8\xb3\xd9\x84\xd8\xa7\xd9\x85", 8));
    assert(!mulib_layout_compute(l, &r, NULL));
    assert(r.lines[0].run.paragraph_level == 1 && fabs(r.lines[0].box.x+r.width-400) < 0.001);
    assert(!mulib_layout_hit_test(&r, 0, 0, &hit) && hit == 8);
    assert(!mulib_layout_hit_test(&r, 500, 0, &hit) && hit == 0);
    mulib_layout_result_destroy(&r);
    assert(!mulib_layout_set_text(l, "\xd8\xb3\xe2\x80\xa8" "abc", 8) && !mulib_layout_compute(l, &r, NULL));
    assert(r.count == 2 && r.lines[1].run.paragraph_level == 1 && r.lines[1].start == 5);
    mulib_layout_result_destroy(&r);
    assert(!mulib_layout_set_text(l, "\xd8\xb3\xe2\x80\xa9" "abc", 8) && !mulib_layout_compute(l, &r, NULL));
    assert(r.count == 2 && r.lines[1].run.paragraph_level == 0);
    mulib_layout_result_destroy(&r);
    assert(!mulib_layout_set_text(l, "\xd8\xb3\xe2\x80\xa8", 5) && !mulib_layout_compute(l, &r, NULL));
    assert(r.count == 2 && r.lines[1].run.paragraph_level == 1);
    mulib_layout_result_destroy(&r);
    assert(!mulib_layout_set_text(l, "a\r\n\n", 4) && !mulib_layout_compute(l, &r, NULL));
    assert(r.count == 3 && r.lines[0].length == 1 && r.lines[0].next == 3 && r.lines[2].start == 4);
    mulib_layout_result_destroy(&r);
    o.width = 1; o.wrap = MULIB_LAYOUT_WRAP_WORD_GRAPHEME;
    assert(!mulib_layout_configure(l, &o));
    assert(!mulib_layout_set_text(l, "a\xcc\x81" "b", 4) && !mulib_layout_compute(l, &r, NULL));
    assert(r.count == 2 && r.lines[0].length == 3 && r.lines[1].start == 3);
    mulib_layout_result_destroy(&r);
    o.wrap = MULIB_LAYOUT_WRAP_WORD;
    assert(!mulib_layout_configure(l, &o) && !mulib_layout_compute(l, &r, NULL) && r.count == 1);
    mulib_layout_result_destroy(&r);
    o.width = NAN; assert(mulib_layout_configure(l, &o) == -1);
    o.width = 50; o.wrap = MULIB_LAYOUT_WRAP_GRAPHEME; o.alignment = MULIB_LAYOUT_CENTER;
    assert(!mulib_layout_configure(l, &o) && !mulib_layout_set_text(l, mixed, strlen(mixed)));
    assert(!mulib_layout_compute(l, &r, NULL) && r.count > 1);
    for (i = 0; i < r.count; ++i) {
        assert(r.lines[i].box.width <= 50 && fabs(r.lines[i].box.x-(50-r.lines[i].box.width)/2) < 0.001);
        for (j = 0; j < r.lines[i].run.count; ++j) assert(r.lines[i].run.glyphs[j].start >= r.lines[i].start);
    }
    mulib_layout_destroy(l); assert(!strcmp(r.text, mixed)); mulib_layout_result_destroy(&r);
    {
        mulib_fontshape_run_t on = {0}, off = {0};
        mulib_ligature_cluster_t *clusters = NULL;
        mulib_fontshape_metrics_t metrics;
        mulib_fontshape_extents_t extents;
        mulib_fontdraw_metrics_t dm;
        mulib_fontdraw_bitmap_t bitmap = {0};
        int32_t *carets = NULL;
        assert(mulib_fontshape_scale(font) == 18*64 && !mulib_fontshape_metrics(font, &metrics) && metrics.ascender > 0);
        assert(!mulib_ligature_shape(font, "office", 6, 3, NULL, &on));
        assert(!mulib_ligature_shape(font, "office", 6, 0, NULL, &off) && on.count < off.count);
        assert(!mulib_ligature_clusters("office", 6, &on, &clusters, &count));
        assert(count == on.count); free(clusters);
        assert(!mulib_fontshape_extents(font, on.glyphs[0].glyph, &extents) && extents.width > 0);
        assert(!mulib_fontshape_carets(font, on.glyphs[1].glyph, "ltr", &carets, &count)); free(carets);
        assert(!mulib_fontdraw_metrics(mulib_glyph_font_draw(paired), on.glyphs[0].glyph, 0, &dm) && dm.width > 0);
        assert(!mulib_glyph_render(paired, on.glyphs[0].glyph, 0, 0, &bitmap) && bitmap.width);
        mulib_fontdraw_bitmap_destroy(&bitmap); mulib_fontshape_run_destroy(&on); mulib_fontshape_run_destroy(&off);
    }
    mulib_glyph_font_close(paired); mulib_uniproc_close(u); mulib_sysfonts_close(catalog);
    return 0;
}
