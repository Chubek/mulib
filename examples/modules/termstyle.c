#define MULIB_IMPL_STRBUF
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
  mulib_strbuf_free (&text); mulib_terminfo_free (&info); return 0;
}
