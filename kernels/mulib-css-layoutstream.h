/** @file mulib-css-layoutstream.h
 * @brief Callback emission of calculated box geometry with stable node
 * indexes. C11 single-header module. Define MULIB_IMPL_CSS_LAYOUTSTREAM in
 * exactly one C translation unit.
 *
 */
#ifndef MULIB_CSS_LAYOUTSTREAM_H
#define MULIB_CSS_LAYOUTSTREAM_H
#include "mulib-css-box.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef int (*mulib_css_layoutstream_fn) (void *ctx, size_t index,
                                            const mulib_css_box_t *box);
  /** @brief See manual/mulib-css-layoutstream.md for this operation's
   * contract. */
  int mulib_css_layoutstream_emit (const mulib_css_box_t *boxes, size_t n,
                                   mulib_css_layoutstream_fn callback,
                                   void *ctx);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_CSS_LAYOUTSTREAM_H */
#ifdef MULIB_IMPL_CSS_LAYOUTSTREAM
#ifndef MULIB_IMPL_CSS_LAYOUTSTREAM_ONCE
#define MULIB_IMPL_CSS_LAYOUTSTREAM_ONCE
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
mulib_css_layoutstream_emit (const mulib_css_box_t *b, size_t n,
                             mulib_css_layoutstream_fn fn, void *ctx)
{
  if ((!b && n) || !fn)
    return 0;
  for (size_t i = 0; i < n; i++)
    if (!mulib_css_box_valid (b[i]))
      return 0;
  for (size_t i = 0; i < n; i++)
    if (!fn (ctx, i, b + i))
      return 0;
  return 1;
}
#endif
#endif
