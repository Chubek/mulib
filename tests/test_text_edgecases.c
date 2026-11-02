/* Backend-independent hostile ABI data, numerical and source-model checks. */
#define MULIB_PGLAYOUT_MAX_MEASUREMENTS 20
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
#define MULIB_IMPL_FONTDRAW
#define MULIB_IMPL_PGLAYOUT
#include "mulib-pglayout.h"
#include "mulib-fontdraw.h"
#include <assert.h>

static int fake_load(void *face, unsigned glyph, int32_t flags) {
    (void)face; (void)glyph; (void)flags; return 0;
}
static int fake_render(void *slot, int mode) { (void)slot; (void)mode; return 0; }
static void bitmap_edges(void) {
    mulib_fontdraw_face_prefix_ face = {0};
    mulib_fontdraw_slot_prefix_ slot = {0};
    mulib_fontdraw_t f = {0};
    mulib_fontdraw_bitmap_t out = {0};
    mulib_fontdraw_outline_t outline = {0};
    unsigned char storage[] = {1,2,0, 3,4,0};
    mulib_fontdraw_vector_ points[] = {{0,0}, {10,20}};
    unsigned char tags[] = {1,1};
    short ends[] = {1};
    f.face = &face; f.load = fake_load; f.render = fake_render; face.glyph = &slot;
    slot.bitmap.buffer = storage; slot.bitmap.width = 2; slot.bitmap.rows = 2;
    slot.bitmap.pitch = -3; slot.bitmap.mode = 2; slot.bitmap.grays = 256;
    assert(!mulib_fontdraw_render(&f, 0, 0, 0, &out));
    assert(out.pixels && out.width == 2 && out.height == 2 && out.stride == 3);
    assert(out.pixels[0] == 3 && out.pixels[1] == 4 && out.pixels[3] == 1);
    /* Copies must not borrow the slot's data. */
    storage[3] = 99; assert(out.pixels[0] == 3); mulib_fontdraw_bitmap_destroy(&out);
    slot.bitmap.pitch = 1;
    assert(mulib_fontdraw_render(&f, 0, 0, 0, &out) == -1 && !out.pixels);
    slot.bitmap.pitch = INT_MIN;
    assert(mulib_fontdraw_render(&f, 0, 0, 0, &out) == -1);
    slot.bitmap.pitch = 3; slot.bitmap.buffer = NULL;
    assert(mulib_fontdraw_render(&f, 0, 0, 0, &out) == -1);
    slot.format = UINT32_C(0x6f75746c);
    slot.outline.points = 2; slot.outline.contours = 1;
    slot.outline.coordinates = points; slot.outline.tags = tags; slot.outline.ends = ends;
    assert(!mulib_fontdraw_outline(&f, 0, 0, &outline) && outline.point_count == 2);
    mulib_fontdraw_outline_destroy(&outline);
    ends[0] = 0; /* final contour must consume all points */
    assert(mulib_fontdraw_outline(&f, 0, 0, &outline) == -1 && !outline.points);
    ends[0] = 1; slot.outline.tags = NULL;
    assert(mulib_fontdraw_outline(&f, 0, 0, &outline) == -1);
    slot.outline.tags = tags; slot.outline.contours = 0;
    assert(mulib_fontdraw_outline(&f, 0, 0, &outline) == -1);
}
static void scalar_roundtrips(void) {
    uint32_t cp;
    for (cp = 0; cp <= 0x10ffff; cp += 127) {
        unsigned char encoded[4];
        char *decoded = NULL;
        uint16_t *u = NULL;
        size_t n, units, bytes, *map = NULL;
        if (cp >= 0xd800 && cp <= 0xdfff) continue;
        n = mulib_utf8_encode(cp, encoded, sizeof encoded); assert(n);
        assert(!mulib_uniproc_utf16((const char *)encoded, n, &u, &units, &map));
        assert(units == (cp > 0xffff ? 2u : 1u) && map[0] == 0 && map[units] == n);
        assert(!mulib_uniproc_utf8(u, units, &decoded, &bytes) && bytes == n && !memcmp(decoded, encoded, n));
        free(u); free(map); free(decoded);
    }
    {
        const char *bad[] = {"\x80", "\xc0\x80", "\xe0\x80\x80", "\xed\xa0\x80", "\xf4\x90\x80\x80", "\xf0\x9f"};
        size_t i;
        for (i = 0; i < sizeof bad/sizeof *bad; ++i) {
            uint16_t *out = NULL;
            size_t units = 777, *map = NULL;
            assert(mulib_uniproc_utf16(bad[i], strlen(bad[i]), &out, &units, &map) == -1);
            assert(!out && !map && units == 777);
        }
    }
}
static void style_edges(void) {
    const char *bad[] = {"width:1.px", "line-height:1.", "font-size:0", "padding:1000001px", "color:#ggg"};
    size_t i;
    mulib_xml_node_t *node = mulib_xml_new(MULIB_XML_ELEMENT, "text");
    mulib_stylesheet_style_t parent = mulib_stylesheet_default(), out;
    assert(node);
    for (i = 0; i < sizeof bad/sizeof *bad; ++i) {
        assert(mulib_xml_set_attr(node, "style", bad[i]));
        out = mulib_stylesheet_default(); out.width = 123;
        assert(mulib_stylesheet_resolve(NULL, node, NULL, &out, NULL) == -1 && out.width == 123);
    }
    assert(mulib_xml_set_attr(node, "style", ""));
    parent.font_size = NAN;
    assert(mulib_stylesheet_resolve(NULL, node, &parent, &out, NULL) == -1);
    parent = mulib_stylesheet_default(); memset(parent.font_family, 'x', sizeof parent.font_family);
    assert(mulib_stylesheet_resolve(NULL, node, &parent, &out, NULL) == -1);
    mulib_xml_destroy(node);
}
static int unwritten(void *ctx, const char *s, size_t n, const mulib_stylesheet_style_t *style, double *width) {
    (void)ctx; (void)s; (void)n; (void)style; (void)width; return 0;
}
static int constant_width(void *ctx, const char *s, size_t n, const mulib_stylesheet_style_t *style, double *width) {
    (void)s; (void)n; (void)style; ++*(size_t *)ctx; *width = 1; return 0;
}
static void model_edges(void) {
    mulib_pglayout_item_t *root = mulib_pglayout_new(MULIB_PGLAYOUT_LAYOUT), *p = root, *child;
    size_t i;
    char *s;
    assert(root);
    for (i = 1; i < 60; ++i) {
        child = mulib_pglayout_new(MULIB_PGLAYOUT_LAYOUT);
        assert(child && !mulib_pglayout_append(p, child)); p = child;
    }
    child = mulib_pglayout_new(MULIB_PGLAYOUT_LAYOUT); assert(child);
    assert(mulib_pglayout_append(p, child) == -1 && !child->parent);
    assert(mulib_pglayout_append(p, root) == -1);
    mulib_pglayout_destroy(child);
    for (i = 0; i < 2; ++i) {
        mulib_pglayout_item_t *copy;
        s = mulib_pglayout_serialize(root, (int)i); assert(s);
        copy = mulib_pglayout_deserialize(s, strlen(s), (int)i); assert(copy);
        free(s); mulib_pglayout_destroy(copy);
    }
    mulib_pglayout_destroy(root);
    root = mulib_pglayout_new(MULIB_PGLAYOUT_IMAGE); assert(root);
    assert(!mulib_pglayout_image_size(root, 1e-300, 1e-300));
    assert(!mulib_pglayout_attr(root, "style", "width:1000000px"));
    assert(!mulib_pglayout_compute(root, NULL, NULL, 100, NULL, NULL));
    {
        mulib_pglayout_box_t before, after;
        assert(!mulib_pglayout_geometry(root, &before, NULL) && fabs(before.height-1e6) < 1e-9);
        assert(!mulib_pglayout_image_size(root, 1e-300, 1e6));
        assert(mulib_pglayout_compute(root, NULL, NULL, 100, NULL, NULL) == -1);
        assert(mulib_pglayout_geometry(root, &after, NULL) == -1);
        assert(!mulib_pglayout_attr(root, "style", "width:1000000px;height:10px"));
        assert(!mulib_pglayout_compute(root, NULL, NULL, 100, NULL, NULL));
        assert(!mulib_pglayout_geometry(root, &before, NULL) && before.height == 10);
    }
    mulib_pglayout_destroy(root);
    {
        mulib_uniproc_t *u = mulib_uniproc_open(NULL, 0);
        if (u) {
            root = mulib_pglayout_new(MULIB_PGLAYOUT_TEXT); assert(root);
            assert(!mulib_pglayout_content(root, "x"));
            assert(mulib_pglayout_compute(root, NULL, u, 100, unwritten, NULL) == -1);
            assert(mulib_pglayout_geometry(root, NULL, NULL) == -1);
            {
                size_t calls = 0;
                assert(!mulib_pglayout_compute(root, NULL, u, 100, constant_width, &calls));
                assert(calls == 1);
                assert(!mulib_pglayout_content(root, "x x x x x x x x x x x x x x x x x x x x x x x"));
                calls = 0;
                assert(mulib_pglayout_compute(root, NULL, u, 100, constant_width, &calls) == -1);
                assert(calls == MULIB_PGLAYOUT_MAX_MEASUREMENTS);
            }
            mulib_pglayout_destroy(root); mulib_uniproc_close(u);
        } else assert(!getenv("MULIB_TEXT_REQUIRE_BACKENDS"));
    }
}
static void mutations(void) {
    const char seed[] = "{\"version\":1,\"root\":{\"type\":\"text\",\"attrs\":{},\"content\":\"hello\"}}";
    const char css[] = "text, .hot {color:#1234;font-size:12.5px;}";
    unsigned state = 42;
    size_t trial, i;
    for (trial = 0; trial < 300; ++trial) {
        char source[sizeof seed];
        mulib_pglayout_item_t *p;
        mulib_stylesheet_t *sheet;
        memcpy(source, seed, sizeof seed);
        state = state*1664525u+1013904223u; i = state % (sizeof seed-1);
        state = state*1664525u+1013904223u; source[i] = (char)(state >> 24);
        p = mulib_pglayout_deserialize(source, sizeof seed-1, 0);
        if (p) {
            char *encoded = mulib_pglayout_serialize(p, 0);
            mulib_pglayout_item_t *copy;
            assert(encoded); copy = mulib_pglayout_deserialize(encoded, strlen(encoded), 0); assert(copy);
            mulib_pglayout_destroy(copy); free(encoded); mulib_pglayout_destroy(p);
        }
        memcpy(source, css, sizeof css);
        state = state*1664525u+1013904223u; i = state % (sizeof css-1);
        state = state*1664525u+1013904223u; source[i] = (char)(state >> 24);
        sheet = mulib_stylesheet_parse(source, sizeof css-1, NULL);
        if (sheet) {
            mulib_xml_node_t *node = mulib_xml_new(MULIB_XML_ELEMENT, "text");
            mulib_stylesheet_style_t style;
            assert(node && !mulib_stylesheet_resolve(sheet, node, NULL, &style, NULL));
            mulib_xml_destroy(node); mulib_stylesheet_destroy(sheet);
        }
    }
}
int main(void) {
    bitmap_edges(); scalar_roundtrips(); style_edges(); model_edges(); mutations(); return 0;
}
