/* Complete, offline css-computed example. */
#define MULIB_IMPL_CSS_COMPUTED
#define MULIB_IMPL_SHYLOCK_CORE
#define MULIB_IMPL_UTF8
#include "mulib-css-computed.h"
#include <assert.h>
#include <math.h>
#include <string.h>

int
main (void)
{
  mulib_css_computed_t style;
  assert (mulib_css_computed_defaults (&style));
  const char *css = "width : 120px; color: blue";
  assert (mulib_css_computed_apply (&style, css, strlen (css))
          && style.width_px == 120);
  return 0;
}
