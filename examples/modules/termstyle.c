#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_XML
#define MULIB_IMPL_CSS
#define MULIB_IMPL_GRID
#define MULIB_IMPL_VTERM
#define MULIB_IMPL_TERMINFO
#define MULIB_IMPL_TERMSTYLE
#include "mulib-termstyle.h"
#include <assert.h>
int main (void)
{
  mulib_terminfo_t info = { 0 }; mulib_termstyle_t style;
  mulib_termstyle_spec_t spec = { MULIB_TERMSTYLE_BOLD, 2, -1 };
  mulib_strbuf_t text = { 0 };
  mulib_terminfo_ansi (&info); /* explicit fallback, never automatic */
  mulib_termstyle_init (&style, &info, 1);
  assert (mulib_termstyle_text (&style, &spec, "success", 7, &text));
  assert (strstr (text.data, "success\033[0m"));
  {
    const char css[] = ".notice {color:#9c6; font-weight:bold}";
    mulib_css_sheet_t *sheet = mulib_css_parse(css,sizeof css-1,NULL);
    mulib_xml_node_t *node = mulib_xml_new(MULIB_XML_ELEMENT,"span");
    mulib_term_style_t computed;
    assert(sheet && node && mulib_xml_set_attr(node,"class","notice"));
    assert(!mulib_termstyle_resolve(sheet,node,NULL,&computed,NULL));
    assert(!mulib_termstyle_write(computed,&info,0,"CSS terminal style",18,&text));
    mulib_xml_destroy(node); mulib_css_destroy(sheet);
  }
  mulib_strbuf_free (&text); mulib_terminfo_free (&info); return 0;
}
