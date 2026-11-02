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
#define MULIB_IMPL_PGLAYOUT
#include "mulib-sysfonts.h"
#include "mulib-pglayout.h"
#include <assert.h>
int main(void) {
    mulib_pglayout_item_t *page = mulib_pglayout_new(MULIB_PGLAYOUT_LAYOUT);
    mulib_pglayout_item_t *text = mulib_pglayout_new(MULIB_PGLAYOUT_TEXT), *copy;
    mulib_sysfonts_t *fonts = mulib_sysfonts_open(NULL);
    mulib_uniproc_t *unicode = mulib_uniproc_open(NULL, 0);
    mulib_sysfonts_font_t match = {0};
    mulib_fontshape_t *font = NULL;
    char *yaml;
    assert(page && text && !mulib_pglayout_content(text, "A styled page with Unicode line breaking."));
    assert(!mulib_pglayout_append(page, text));
    if (fonts && unicode && !mulib_sysfonts_match(fonts, "sans", &match)) {
        font = mulib_fontshape_open(match.path, (unsigned)match.index, 16*64, NULL);
        if (font) {
            mulib_pglayout_box_t box;
            mulib_pglayout_fontset_t set = {unicode, &font, 1, "en", -1};
            assert(!mulib_pglayout_compute_shaped(page, NULL, &set, 16, 160));
            assert(!mulib_pglayout_geometry(page, &box, NULL));
            printf("Page: %.1f x %.1f pixels\n", box.width, box.height);
            {
                const mulib_pglayout_shaped_line_t *line = mulib_pglayout_shaped_line(text, 0);
                assert(line); printf("First line: %zu shaped runs\n", line->count);
            }
        }
    }
    yaml = mulib_pglayout_serialize(page, 1); assert(yaml); puts(yaml);
    copy = mulib_pglayout_deserialize(yaml, strlen(yaml), 1); assert(copy);
    free(yaml); mulib_pglayout_destroy(copy); mulib_pglayout_destroy(page);
    mulib_fontshape_close(font); mulib_sysfonts_font_destroy(&match);
    mulib_sysfonts_close(fonts); mulib_uniproc_close(unicode); return 0;
}
