#define MULIB_IMPL_LAYOUTML
#define MULIB_IMPL_LAYOUT
#define MULIB_IMPL_XML
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_DYNLOAD
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_JSON
#define MULIB_IMPL_YAML
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
#include "mulib-layoutml.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
int
main (void)
{
  const char *s = "<layout direction='row'><item weight='1'/></layout>";
  mulib_layout_rect_t p = { 0, 0, 100, 20 }, *r;
  size_t n;
  assert (mulib_layoutml_parse (s, strlen (s), p, &r, &n, NULL) && n == 1
          && r[0].width == 100);
  free (r);
  return 0;
}
