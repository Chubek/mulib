#include <stdlib.h>
#include <assert.h>
#include <string.h>
static size_t allocation, fail_at = (size_t)-1;
static void *test_malloc(size_t n) { return allocation++ == fail_at ? NULL : malloc(n); }
static void *test_calloc(size_t n, size_t s) { return allocation++ == fail_at ? NULL : calloc(n, s); }
static void *test_realloc(void *p, size_t n) { return allocation++ == fail_at ? NULL : realloc(p, n); }
#define malloc test_malloc
#define calloc test_calloc
#define realloc test_realloc
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_XML
#define MULIB_IMPL_HTML
#define MULIB_IMPL_CSS
#include "mulib-html.h"
#include "mulib-css.h"
#undef malloc
#undef calloc
#undef realloc

int main(void) {
    const char *source = "<!DOCTYPE html><HTML><HEAD><TITLE>A &amp; B</TITLE></HEAD><BODY>"
      "<P ID=one class='note' hidden data-x=ok>Hello&nbsp;&#x633;<BR>world</P><PRE>\n\nx</PRE></BODY></HTML>";
    const char *css = "#missing,p { color:red; --X:a } .note {color:green} #one {color:blue !important}"
      "p {margin:1.5em; --x:b}";
    mulib_xml_error_t error;
    mulib_xml_node_t *doc = mulib_html_parse(source, strlen(source), 0, &error), *p;
    mulib_css_sheet_t *sheet;
    mulib_css_style_t *inline_style, *resolved, *copy;
    mulib_css_specificity_t rank;
    char *serialized, *text;
    size_t n, i, total;
    assert(doc && !error.message);
    p = mulib_xml_first(mulib_html_body(doc));
    assert(!strcmp(mulib_xml_attr(p, "hidden"), "") && !strcmp(mulib_xml_attr(p, "data-x"), "ok"));
    text = mulib_html_text(p, &n); assert(text && !strcmp(text, "Hello\xc2\xa0\xd8\xb3\nworld") && n == strlen(text)); free(text);
    serialized = mulib_html_serialize(doc, &n); assert(serialized);
    {
        mulib_xml_node_t *again = mulib_html_parse(serialized, n, 0, &error);
        char *roundtrip;
        assert(again); roundtrip = mulib_html_serialize(again, NULL); assert(roundtrip && !strcmp(serialized, roundtrip));
        free(roundtrip); mulib_xml_destroy(again);
    }
    free(serialized);
    for (i = 0; i < strlen(source); ++i) {
        mulib_xml_node_t *prefix = mulib_html_parse(source, i, 0, &error);
        if (prefix) mulib_xml_destroy(prefix);
        else assert(error.message && error.offset <= i);
    }
    {
        mulib_strbuf_t deep = {0}; mulib_xml_node_t *node;
        for (i = 0; i < 127; ++i) assert(mulib_strbuf_append(&deep, "<a>"));
        assert(mulib_strbuf_append(&deep, "x"));
        for (i = 0; i < 127; ++i) assert(mulib_strbuf_append(&deep, "</a>"));
        node = mulib_html_parse(deep.data, deep.length, 1, &error); assert(node);
        text = mulib_html_text(node, NULL); assert(text && !strcmp(text, "x")); free(text);
        serialized = mulib_html_serialize(node, NULL); assert(serialized); free(serialized); mulib_xml_destroy(node);
        mulib_strbuf_free(&deep); mulib_strbuf_init(&deep);
        for (i = 0; i < 128; ++i) assert(mulib_strbuf_append(&deep, "<a>"));
        for (i = 0; i < 128; ++i) assert(mulib_strbuf_append(&deep, "</a>"));
        assert(!mulib_html_parse(deep.data, deep.length, 1, &error)); mulib_strbuf_free(&deep);
        node = mulib_html_parse("<pre>&#10;x</pre>", strlen("<pre>&#10;x</pre>"), 1, &error); assert(node);
        text = mulib_html_text(node, NULL); assert(text && !strcmp(text, "x")); free(text); mulib_xml_destroy(node);
    }
    assert(!mulib_html_add(mulib_xml_next(mulib_xml_first(p)), "span", NULL)); /* br */
    sheet = mulib_css_parse(css, strlen(css), &error); assert(sheet);
    inline_style = mulib_css_parse_declarations("color:purple; padding:2pt", strlen("color:purple; padding:2pt"), &error); assert(inline_style);
    assert(mulib_css_specificity(p, "#missing, p.note", &rank) == 1 && rank.ids == 0 && rank.classes == 1 && rank.types == 1);
    resolved = mulib_css_cascade(sheet, p, inline_style, &error); assert(resolved);
    assert(!strcmp(mulib_css_get(resolved, "COLOR"), "blue") && !strcmp(mulib_css_get(resolved, "--X"), "a"));
    assert(!strcmp(mulib_css_get(resolved, "--x"), "b") && !strcmp(mulib_css_get(resolved, "padding"), "2pt"));
    serialized = mulib_css_serialize_declarations(resolved); assert(serialized);
    copy = mulib_css_parse_declarations(serialized, strlen(serialized), &error); assert(copy);
    assert(mulib_css_count(copy) == mulib_css_count(resolved)); free(serialized); mulib_css_style_destroy(copy);
    mulib_css_style_destroy(resolved);
    {
        mulib_css_length_t length = {9, MULIB_CSS_PX}; double px = 99;
        assert(!mulib_css_parse_length(" 1.5EM ", &length) && !mulib_css_length_pixels(length, 20, 16, 400, &px) && px == 30);
        assert(!mulib_css_parse_length("-25%", &length) && !mulib_css_length_pixels(length, 20, 16, 400, &px) && px == -100);
        assert(!mulib_css_parse_length("12pt", &length) && !mulib_css_length_pixels(length, 20, 16, 400, &px) && px == 16);
        assert(!mulib_css_parse_length("auto", &length) && mulib_css_length_pixels(length, 20, 16, 400, &px) == -1 && px == 16);
        assert(mulib_css_parse_length("+auto", &length) == -1 && mulib_css_parse_length("1", &length) == -1);
        assert(mulib_css_parse_length("1e999px", &length) == -1);
        copy = mulib_css_parse_declarations("\fcolor:\fred\f", 12, &error); assert(copy); mulib_css_style_destroy(copy);
    }
    {
        const char *bad[] = {"<p>", "<p>x</div>", "<img>x</img>", "<p a=1 A=2></p>", "<script>x</script>",
            "<p>&unknown;</p>", "<p>&#0;</p>", "<p>&#x110000;</p>", "<p/>", "<p a='x></p>", "<!--x", "<svg></svg>"};
        for (i = 0; i < sizeof bad/sizeof *bad; ++i) assert(!mulib_html_parse(bad[i], strlen(bad[i]), 1, &error) && error.message);
        assert(!mulib_html_parse(NULL, 1, 1, &error));
        { mulib_xml_node_t *empty = mulib_html_parse(NULL, 0, 1, &error); assert(empty); mulib_xml_destroy(empty); }
    }
    allocation = 0;
    { mulib_xml_node_t *probe = mulib_html_parse(source, strlen(source), 0, NULL); assert(probe); total = allocation; mulib_xml_destroy(probe); }
    for (i = 0; i < total; ++i) {
        mulib_xml_node_t *probe; allocation = 0; fail_at = i;
        probe = mulib_html_parse(source, strlen(source), 0, &error); assert(!probe && error.message);
    }
    fail_at = (size_t)-1; allocation = 0;
    resolved = mulib_css_cascade(sheet, p, inline_style, NULL); assert(resolved); total = allocation; mulib_css_style_destroy(resolved);
    for (i = 0; i < total; ++i) {
        allocation = 0; fail_at = i; resolved = mulib_css_cascade(sheet, p, inline_style, &error);
        assert(!resolved && error.message);
    }
    fail_at = (size_t)-1;
    mulib_css_style_destroy(inline_style); mulib_css_destroy(sheet); mulib_xml_destroy(doc);
    return 0;
}
