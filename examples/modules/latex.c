#define MULIB_IMPL_LATEX
#define MULIB_IMPL_TEX
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#include "mulib-latex.h"
#include <assert.h>
int
main (void)
{
  mulib_latex_t d = { 0 };
  assert (mulib_latex_begin (&d, "Example"));
  assert (mulib_latex_section (&d, 1, "Introduction"));
  assert (mulib_latex_paragraph (&d, "Text & more.", 12));
  assert (mulib_latex_finish (&d));
  mulib_latex_free (&d);
  return 0;
}
