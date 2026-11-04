#include <stdlib.h>
#include <assert.h>
#include <string.h>
static size_t allocation, fail_at = (size_t)-1;
static void *xll_malloc(size_t n) { return allocation++ == fail_at ? NULL : malloc(n); }
static void *xll_calloc(size_t n, size_t s) { return allocation++ == fail_at ? NULL : calloc(n, s); }
static void *xll_realloc(void *p, size_t n) { return allocation++ == fail_at ? NULL : realloc(p, n); }
#define malloc xll_malloc
#define calloc xll_calloc
#define realloc xll_realloc
#include "fixtures/xll_implementation.h"
#undef malloc
#undef calloc
#undef realloc
#include "fixtures/xll_source.h"

static void equal(const mulib_xll_block_t *a, const mulib_xll_block_t *b) {
    assert(a && b && !strcmp(a->name, b->name) && a->length == b->length && !memcmp(a->text, b->text, a->length));
    assert(!strcmp(a->font_family, b->font_family) && !strcmp(a->language, b->language));
    assert(a->font_size == b->font_size && a->x == b->x && a->y == b->y);
    assert(a->options.width == b->options.width && a->options.spacing == b->options.spacing &&
        a->options.wrap == b->options.wrap && a->options.alignment == b->options.alignment && a->options.base == b->options.base);
}
int main(int argc, char **argv) {
    mulib_xll_error_t error;
    mulib_xll_t *program = mulib_xll_parse(xll_source, strlen(xll_source), &error), *copy, *api;
    mulib_xll_block_t block = mulib_xll_block_default();
    char *source, *c;
    size_t length, i, total;
    assert(program && !error.message && mulib_xll_count(program) == 3);
    if (argc == 2 && !strcmp(argv[1], "--emit")) {
        int result = mulib_xll_write_c(program, "fixture", stdout, &error);
        mulib_xll_destroy(program); return result ? 1 : 0;
    }
    if (argc == 2 && !strcmp(argv[1], "--emit-empty")) {
        int result;
        mulib_xll_destroy(program); program = mulib_xll_new(); assert(program);
        result = mulib_xll_write_c(program, "empty", stdout, &error);
        mulib_xll_destroy(program); return result ? 1 : 0;
    }
    if (argc == 2 && !strcmp(argv[1], "--emit-long")) {
        char text[8192]; int result;
        memset(text, 'x', sizeof text); text[4000] = 0;
        block.name = "long_text"; block.text = text; block.length = sizeof text;
        assert(!mulib_xll_add(program, &block, &error));
        result = mulib_xll_write_c(program, "longtext", stdout, &error);
        mulib_xll_destroy(program); return result ? 1 : 0;
    }
    assert(mulib_xll_find(program, "greeting") == 0 && mulib_xll_find(program, "absent") == SIZE_MAX);
    assert(!mulib_xll_get(program, 3) && !mulib_xll_count(NULL));
    assert(mulib_xll_get(program, 1)->text[1] == 0 && mulib_xll_get(program, 1)->length > 2);
    source = mulib_xll_serialize(program, &length); assert(source && length == strlen(source));
    copy = mulib_xll_parse(source, length, &error); assert(copy);
    for (i = 0; i < 3; ++i) equal(mulib_xll_get(program, i), mulib_xll_get(copy, i));
    free(source); mulib_xll_destroy(copy);
    api = mulib_xll_new(); assert(api);
    for (i = 0; i < 3; ++i) assert(!mulib_xll_add(api, mulib_xll_get(program, i), &error));
    c = mulib_xll_emit_c(program, "fixture", &length, &error); assert(c && !error.message);
    assert(strstr(c, "#include \"mulib-layout.h\"") && strstr(c, "mulib_layout_create") && !strstr(c, "mulib_xll_parse"));
    source = mulib_xll_emit_c(api, "fixture", NULL, &error); assert(source && !strcmp(source, c)); free(source); free(c);
    assert(!mulib_xll_set(api, 0, mulib_xll_get(api, 0), &error));
    assert(mulib_xll_add(api, mulib_xll_get(api, 0), &error) == -1);
    assert(mulib_xll_set(api, 99, &block, &error) == -1);
    block.name = "valid"; block.text = "owned"; block.length = 5;
    assert(!mulib_xll_set(api, 0, &block, &error) && mulib_xll_find(api, "greeting") == SIZE_MAX);
    block.options.width = NAN; assert(mulib_xll_add(api, &block, &error) == -1 && mulib_xll_count(api) == 3);
    assert(!mulib_xll_emit_c(program, "evil;", &length, &error) && !length && error.message);
    assert(!mulib_xll_emit_c(program, "_reserved", NULL, &error));
    assert(mulib_xll_write_c(program, "ok", NULL, &error) == -1);
    {
        char text[] = {'a', 0, 'b'}, name[] = "copied", family[] = "sans";
        mulib_xll_block_t owned = mulib_xll_block_default();
        owned.name = name; owned.text = text; owned.length = sizeof text; owned.font_family = family;
        assert(!mulib_xll_add(api, &owned, &error));
        memset(text, 'x', sizeof text); name[0] = 'X'; family[0] = 'X';
        assert(!strcmp(mulib_xll_get(api, 3)->name, "copied") && !strcmp(mulib_xll_get(api, 3)->font_family, "sans"));
        assert(mulib_xll_get(api, 3)->length == 3 && !memcmp(mulib_xll_get(api, 3)->text, "a\0b", 3));
        owned = mulib_xll_block_default(); owned.name = "invalid"; owned.options.width = INFINITY;
        assert(mulib_xll_add(api, &owned, &error) == -1);
        owned = mulib_xll_block_default(); owned.name = "invalid"; owned.text = NULL; owned.length = 16u*1024u*1024u+1;
        assert(mulib_xll_add(api, &owned, &error) == -1);
        assert(!mulib_xll_parse(NULL, 128u*1024u*1024u+1, &error));
    }
    {
        FILE *file = tmpfile(); long before;
        assert(file); before = ftell(file);
        assert(mulib_xll_write_c(program, ";", file, &error) == -1 && ftell(file) == before);
        assert(!mulib_xll_write_c(program, "ok", file, &error) && ftell(file) > before); assert(!fclose(file));
    }
    {
        const char *bad[] = {"", "xll 2 {}", "xll 1.0 {}", "xll 1 {} extra", "xll 1 { layout a { width 1; width 2; } }",
            "xll 1 { layout a {} layout a {} }", "xll 1 { layout a { unknown 1; } }", "xll 1 { layout a {width -1;} }",
            "xll 1 { layout a {wrap invalid;} }", "xll 1 { layout a {font \"sans\" 0;} }", "xll 1 { layout a {spacing 0;} }",
            "xll 1 { layout a {text \"\\uD800\";} }", "xll 1 { layout a {font \"a\\u0000b\" 16;} }",
            "xll 1 { layout a {width 1e999;} }", "xll 1 { layout a {text \"x\"} }", "xll 1 {/* open", "xll 1 {layout a {text \"open;}}"};
        for (i = 0; i < sizeof bad/sizeof *bad; ++i) {
            assert(!mulib_xll_parse(bad[i], strlen(bad[i]), &error) && error.message && error.line && error.column && error.offset <= strlen(bad[i]));
        }
        assert(!mulib_xll_parse(NULL, 1, &error));
        assert(!mulib_xll_parse("\xc0\x80", 2, &error));
        assert(!mulib_xll_parse("xll 1 {}\0", 9, &error));
        assert(!mulib_xll_parse("xll 1 {\r\n  bad\n}", strlen("xll 1 {\r\n  bad\n}"), &error) && error.line == 2 && error.column == 3);
        copy = mulib_xll_parse("xll 1 {}", 8, &error); assert(copy && !mulib_xll_count(copy)); mulib_xll_destroy(copy);
        for (i = 0; i < strlen(xll_source); ++i) {
            copy = mulib_xll_parse(xll_source, i, &error);
            if (copy) mulib_xll_destroy(copy); else assert(error.message && error.offset <= i);
        }
    }
    {
        char name[64]; mulib_xll_t *many = mulib_xll_new(); assert(many);
        block = mulib_xll_block_default();
        for (i = 0; i < 1024; ++i) { snprintf(name, sizeof name, "item%zu", i); block.name = name; assert(!mulib_xll_add(many, &block, NULL)); }
        block.name = "extra"; assert(mulib_xll_add(many, &block, &error) == -1);
        mulib_xll_destroy(many);
        memset(name, 'a', 63); name[63] = 0; block.name = name;
        assert(!mulib_xll_add(api, &block, &error));
    }
    allocation = 0;
    copy = mulib_xll_parse(xll_source, strlen(xll_source), NULL); assert(copy); total = allocation; mulib_xll_destroy(copy);
    for (i = 0; i < total; ++i) {
        allocation = 0; fail_at = i; copy = mulib_xll_parse(xll_source, strlen(xll_source), &error);
        assert(!copy && error.message);
    }
    fail_at = (size_t)-1; allocation = 0;
    c = mulib_xll_emit_c(program, "fixture", NULL, NULL); assert(c); total = allocation; free(c);
    for (i = 0; i < total; ++i) {
        allocation = 0; fail_at = i; c = mulib_xll_emit_c(program, "fixture", &length, &error);
        assert(!c && !length && error.message);
    }
    fail_at = (size_t)-1;
    allocation = 0;
    source = mulib_xll_serialize(program, &length); assert(source); total = allocation; free(source);
    for (i = 0; i < total; ++i) {
        allocation = 0; fail_at = i; source = mulib_xll_serialize(program, &length);
        assert(!source && !length);
    }
    fail_at = (size_t)-1;
    for (i = 0; i < 4; ++i) {
        allocation = 0; fail_at = i;
        assert(mulib_xll_set(program, 0, mulib_xll_get(program, 1), &error) == -1); /* duplicate, no allocation */
        block = *mulib_xll_get(program, 0); block.text = "replacement"; block.length = 11;
        assert(mulib_xll_set(program, 0, &block, &error) == -1);
        assert(!strcmp(mulib_xll_get(program, 0)->text, "Hello سلام world"));
    }
    fail_at = (size_t)-1;
    mulib_xll_destroy(program); mulib_xll_destroy(api); return 0;
}
