/* Complete, offline css-parser example. */
#define MULIB_IMPL_CSS_PARSER
#define MULIB_IMPL_SHYLOCK_CORE
#define MULIB_IMPL_UTF8
#include "mulib-css-parser.h"
#include <assert.h>
#include <math.h>
#include <string.h>

int
main (void)
{
  mulib_css_parsed_sheet_t sheet = { 0 };
  const char *css = "/* author */ p { color: red; }";
  assert (mulib_css_parse_sheet (css, strlen (css), 10, &sheet)
          && sheet.len == 1);
  mulib_css_sheet_free (&sheet);
  return 0;
}
