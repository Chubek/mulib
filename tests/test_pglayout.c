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
#define MULIB_IMPL_PGLAYOUT
#include "mulib-pglayout.h"
#include <assert.h>

static int measure(void *context, const char *text, size_t length, const mulib_stylesheet_style_t *style, double *width) {
    size_t i = 0, used, count = 0;
    uint32_t cp;
    (void)style;
    if (context) return -1;
    while (i < length) { assert(mulib_utf8_decode((const unsigned char *)text+i, length-i, &cp, &used)); i += used; ++count; }
    *width = (double)count*10; return 0;
}
int main(void) {
    const char css[] = "text { font-size: 10px; line-height: 2; color: #123; } .hot {color: #f00;} #title {color: #00f;} layout > text.hot {padding: 2px;} text, #absent {width: auto;}";
    mulib_stylesheet_t *sheet = mulib_stylesheet_parse(css, sizeof css-1, NULL);
    mulib_stylesheet_style_t style;
    mulib_xml_node_t *node = mulib_xml_new(MULIB_XML_ELEMENT, "text");
    mulib_pglayout_item_t *root, *text, *image, *path, *copy;
    mulib_pglayout_box_t box, previous;
    mulib_uniproc_t *u;
    const mulib_pglayout_line_t *lines;
    size_t count;
    char *serialized, *again;
    int yaml;
    double points[] = {0, 0, 10, 20, 30, 0};
    assert(sheet && node);
    assert(mulib_xml_set_attr(node, "class", "hot") && mulib_xml_set_attr(node, "id", "title"));
    assert(!mulib_stylesheet_resolve(sheet, node, NULL, &style, NULL) && style.color == 0x0000ffff);
    assert(mulib_xml_set_attr(node, "style", "color: #fff !important; width: 12px"));
    assert(!mulib_stylesheet_resolve(sheet, node, NULL, &style, NULL) && style.color == UINT32_MAX && style.width == 12);
    mulib_xml_destroy(node);
    {
        const char cascade[] = "text, #missing {color:#00f;} .hot {color:#f00;} [data-x='a,b'] {width:5px !important;}";
        mulib_stylesheet_t *rules = mulib_stylesheet_parse(cascade, sizeof cascade-1, NULL);
        mulib_stylesheet_style_t inherited;
        node = mulib_xml_new(MULIB_XML_ELEMENT, "text");
        assert(rules && node && mulib_xml_set_attr(node, "class", "hot"));
        assert(mulib_xml_set_attr(node, "data-x", "a,b") && mulib_xml_set_attr(node, "style", "width:9px"));
        assert(!mulib_stylesheet_resolve(rules, node, NULL, &style, NULL));
        assert(style.color == UINT32_C(0xff0000ff) && style.width == 5);
        assert(!mulib_stylesheet_resolve(NULL, node, &style, &inherited, NULL));
        assert(inherited.color == style.color && inherited.width == 9 && inherited.padding == 0);
        assert(mulib_xml_set_attr(node, "style", "width:9px !important"));
        assert(!mulib_stylesheet_resolve(rules, node, NULL, &style, NULL) && style.width == 9);
        mulib_xml_destroy(node); mulib_stylesheet_destroy(rules);
    }
    assert(!mulib_stylesheet_parse("text{unknown:1}", 15, NULL));
    assert(!mulib_stylesheet_parse("text{width:-1px}", 16, NULL));
    assert(!mulib_stylesheet_parse("text{color:#xyz}", 16, NULL));
    root = mulib_pglayout_new(MULIB_PGLAYOUT_LAYOUT); text = mulib_pglayout_new(MULIB_PGLAYOUT_TEXT);
    image = mulib_pglayout_new(MULIB_PGLAYOUT_IMAGE); path = mulib_pglayout_new(MULIB_PGLAYOUT_PATH);
    assert(root && text && image && path);
    assert(!mulib_pglayout_content(text, "ab cd\r\nef\n"));
    assert(!mulib_pglayout_attr(text, "class", "hot"));
    assert(!mulib_pglayout_content(image, "asset://example.png") && !mulib_pglayout_image_size(image, 10, 20));
    assert(!mulib_pglayout_path(path, points, 3, 1));
    assert(!mulib_pglayout_append(root, text) && !mulib_pglayout_append(root, image) && !mulib_pglayout_append(root, path));
    assert(mulib_pglayout_append(root, root) == -1 && mulib_pglayout_append(root, text) == -1);
    assert(mulib_pglayout_content(text, "\xff") == -1);
    for (yaml = 0; yaml < 2; ++yaml) {
        serialized = mulib_pglayout_serialize(root, yaml); assert(serialized);
        copy = mulib_pglayout_deserialize(serialized, strlen(serialized), yaml); assert(copy);
        assert(mulib_pglayout_count(copy) == 3 && !strcmp(mulib_pglayout_text(mulib_pglayout_child(copy, 0)), "ab cd\r\nef\n"));
        again = mulib_pglayout_serialize(copy, yaml); assert(again && !strcmp(serialized, again));
        free(again); free(serialized); mulib_pglayout_destroy(copy);
    }
    assert(!mulib_pglayout_deserialize("{}", 2, 0));
    assert(!mulib_pglayout_deserialize("version: 2\nroot: {}", 19, 1));
    assert(mulib_pglayout_geometry(text, &box, NULL) == -1);
    {
        const char *bad[] = {
            "{\"version\":1,\"root\":{\"type\":\"text\",\"attrs\":{},\"content\":\"a\\u0000b\"}}",
            "{\"version\":1,\"root\":{\"type\":\"path\",\"attrs\":{},\"points\":[1],\"closed\":true}}",
            "{\"version\":1,\"root\":{\"type\":\"text\",\"attrs\":{},\"content\":\"\",\"extra\":0}}"
        };
        size_t i;
        for (i = 0; i < sizeof bad/sizeof *bad; ++i) assert(!mulib_pglayout_deserialize(bad[i], strlen(bad[i]), 0));
    }
    u = mulib_uniproc_open(NULL, 0);
    if (u) {
        assert(!mulib_pglayout_compute(root, sheet, u, 44, measure, NULL));
        lines = mulib_pglayout_lines(text, &count); assert(lines && count == 4);
        assert(lines[0].start == 0 && lines[0].length == 3 && lines[0].box.width == 30);
        assert(lines[1].start == 3 && lines[1].length == 2 && lines[2].start == 7 && lines[3].start == 10);
        assert(!mulib_pglayout_geometry(text, &box, &style) && box.width == 44 && box.height == 84 && style.padding == 2);
        assert(!mulib_pglayout_geometry(image, &box, NULL) && box.y == 84 && box.height == 20);
        assert(!mulib_pglayout_geometry(root, &previous, NULL) && previous.height == 124);
        assert(mulib_pglayout_compute(root, sheet, u, 44, measure, root) == -1);
        assert(!mulib_pglayout_geometry(root, &box, NULL) && box.height == previous.height);
        assert(!mulib_pglayout_content(text, "abcdef"));
        assert(mulib_pglayout_geometry(root, &box, NULL) == -1);
        assert(!mulib_pglayout_compute(root, sheet, u, 24, measure, NULL));
        lines = mulib_pglayout_lines(text, &count); assert(lines && count == 3 && lines[0].length == 2);
        assert(!mulib_pglayout_content(text, "e\xcc\x81" "x"));
        assert(!mulib_pglayout_compute(root, sheet, u, 14, measure, NULL));
        lines = mulib_pglayout_lines(text, &count);
        assert(lines && count == 2 && lines[0].length == 3 && lines[0].box.width == 20);
        assert(!mulib_pglayout_attr(text, "style", "display:none"));
        assert(!mulib_pglayout_compute(root, sheet, u, 44, measure, NULL));
        assert(!mulib_pglayout_geometry(image, &box, NULL) && box.y == 0);
        mulib_uniproc_close(u);
    } else { assert(!getenv("MULIB_TEXT_REQUIRE_BACKENDS")); puts("SKIP: ICU layout tests"); }
    mulib_pglayout_destroy(root); mulib_stylesheet_destroy(sheet);
    return 0;
}
