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
#include "mulib-glyph.h"
#include <assert.h>
int main(void) {
    mulib_sysfonts_t *catalog = mulib_sysfonts_open(NULL);
    mulib_uniproc_t *unicode = mulib_uniproc_open(NULL, 0);
    mulib_glyph_font_t *font = catalog ? mulib_glyph_font_match(catalog, "sans", 18) : NULL;
    if (font && unicode) {
        mulib_fontshape_t *shape = mulib_glyph_font_shape(font);
        mulib_pglayout_fontset_t fonts = {unicode, &shape, 1, "en", -1};
        mulib_glyph_run_t run = {0};
        mulib_fontdraw_bitmap_t bitmap = {0};
        assert(!mulib_glyph_shape(&fonts, "office", 6, 0, 6, &run) && run.count);
        assert(!mulib_glyph_render(font, run.glyphs[0].id, 0, 0, &bitmap));
        mulib_fontdraw_bitmap_destroy(&bitmap); mulib_glyph_run_destroy(&run);
    }
    mulib_glyph_font_close(font); mulib_uniproc_close(unicode); mulib_sysfonts_close(catalog);
    return 0;
}
