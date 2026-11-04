#define MULIB_IMPL_LAYOUT
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
#include "mulib-layout.h"
#include <assert.h>
int
main (void)
{
  mulib_layout_rect_t p = { 0, 0, 100, 20 }, r[2];
  double weights[] = { 1, 3 };
  assert (mulib_layout_distribute (p, MULIB_LAYOUT_ROW, 4, weights, 2, r));
  assert (r[0].width == 24 && r[1].x == 28);
  {
    mulib_sysfonts_t *catalog = mulib_sysfonts_open(NULL);
    mulib_uniproc_t *unicode = mulib_uniproc_open(NULL, 0);
    mulib_glyph_font_t *paired = catalog ? mulib_glyph_font_match(catalog, "sans", 18) : NULL;
    if (paired && unicode) {
      mulib_fontshape_t *font = mulib_glyph_font_shape(paired);
      mulib_pglayout_fontset_t fonts = {unicode, &font, 1, "en", -1};
      mulib_layout_t *layout = mulib_layout_create(&fonts);
      mulib_layout_options_t options = mulib_layout_default();
      mulib_layout_result_t result = {0};
      const char *text = "Hello world: Unicode paragraph layout.";
      assert(layout); options.width = 160;
      assert(!mulib_layout_configure(layout, &options));
      assert(!mulib_layout_set_text(layout, text, strlen(text)));
      assert(!mulib_layout_compute(layout, &result, NULL) && result.count > 1);
      mulib_layout_result_destroy(&result); mulib_layout_destroy(layout);
    }
    mulib_glyph_font_close(paired); mulib_uniproc_close(unicode); mulib_sysfonts_close(catalog);
  }
  return 0;
}
