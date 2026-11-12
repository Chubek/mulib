/* Complete, offline css-layoutstream example. */
#define MULIB_IMPL_CSS_BOX
#define MULIB_IMPL_CSS_LAYOUTSTREAM
#include "mulib-css-layoutstream.h"
#include <assert.h>
#include <math.h>
#include <string.h>

static int
on_layout (void *ctx, size_t index, const mulib_css_box_t *box)
{
  assert (index == *(size_t *)ctx && box->width == 10);
  (*(size_t *)ctx)++;
  return 1;
}

int
main (void)
{
  mulib_css_box_t boxes[] = { { 0, 0, 10, 20 } };
  size_t count = 0;
  assert (mulib_css_layoutstream_emit (boxes, 1, on_layout, &count)
          && count == 1);
  return 0;
}
