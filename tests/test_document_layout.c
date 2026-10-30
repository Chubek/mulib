#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_TEX
#define MULIB_IMPL_LATEX
#define MULIB_IMPL_LAYOUT
#define MULIB_IMPL_XML
#define MULIB_IMPL_LAYOUTML
#include "mulib-latex.h"
#include "mulib-layoutml.h"
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
int
main (void)
{
  mulib_tex_t scan;
  mulib_tex_token_t tok;
  mulib_latex_t doc = { 0 };
  mulib_strbuf_t escaped = { 0 };
  const char *s = "\\hello{a}%comment\n";
  int kinds[] = { 2, 3, 1, 4, 5, 6 };
  size_t i, n;
  mulib_layout_rect_t p = { 0, 0, 100, 20 }, rect[2], *loaded = NULL;
  double w[] = { 1, 3 };
  mulib_xml_error_t err;
  const char *xml = "<layout direction='row' gap='4'><item weight='1'/><item "
                    "weight='3'/></layout>";
  assert (mulib_tex_init (&scan, s, strlen (s)));
  for (i = 0; i < 6; ++i)
    assert (mulib_tex_next (&scan, &tok) == 1 && tok.kind == kinds[i]);
  assert (!mulib_tex_next (&scan, &tok));
  assert (!mulib_tex_init (&scan, "\0", 1));
  assert (mulib_tex_init (&scan, "\\", 1) && mulib_tex_next (&scan, &tok) == -1
          && !scan.position);
  assert (mulib_tex_escape (&escaped, "a_%\\{}", 6)
          && strstr (escaped.data, "\\_")
          && strstr (escaped.data, "textbackslash"));
  assert (mulib_latex_begin (&doc, "Title & more"));
  assert (!mulib_latex_begin (&doc, "twice"));
  assert (mulib_latex_section (&doc, 1, "A%"));
  assert (mulib_latex_paragraph (&doc, "Hello", 5));
  assert (mulib_latex_finish (&doc) && mulib_latex_finish (&doc));
  assert (!mulib_latex_paragraph (&doc, "x", 1));
  assert (strstr (doc.source.data, "Title \\& more")
          && strstr (doc.source.data, "\\end{document}"));
  assert (mulib_layout_distribute (p, 0, 4, w, 2, rect) && rect[0].width == 24
          && rect[1].x == 28 && rect[1].width == 72);
  w[1] = NAN;
  assert (!mulib_layout_distribute (p, 0, 4, w, 2, rect));
  assert (mulib_layoutml_parse (xml, strlen (xml), p, &loaded, &n, &err)
          && n == 2 && loaded[1].x == 28);
  free (loaded);
  xml = "<layout direction='row'><bad weight='1'/></layout>";
  assert (!mulib_layoutml_parse (xml, strlen (xml), p, &loaded, &n, &err)
          && err.message);
  xml = "<layout direction='row'><item weight='0'/></layout>";
  assert (!mulib_layoutml_parse (xml, strlen (xml), p, &loaded, &n, &err));
  xml = "<!-- unsupported --><layout direction='row'/>";
  assert (!mulib_layoutml_parse (xml, strlen (xml), p, &loaded, &n, &err));
  mulib_strbuf_free (&escaped);
  mulib_latex_free (&doc);
  return 0;
}
