/** @file mulib-css-computed.h
 * @brief Selected CSS declaration lookup with inherited color and font-size
 * fields. C11 single-header module. Define MULIB_IMPL_CSS_COMPUTED in exactly
 * one C translation unit.
 *
 */
#ifndef MULIB_CSS_COMPUTED_H
#define MULIB_CSS_COMPUTED_H
#include "mulib-bounded-text.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct mulib_css_computed
  {
    char display[24], color[64];
    double font_size_px, width_px, height_px;
  } mulib_css_computed_t;
  /** @brief See manual/mulib-css-computed.md for this operation's contract. */
  int mulib_css_computed_defaults (mulib_css_computed_t *style);
  /** @brief See manual/mulib-css-computed.md for this operation's contract. */
  int mulib_css_computed_apply (mulib_css_computed_t *style,
                                const char *declarations, size_t n);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_CSS_COMPUTED_H */
#ifdef MULIB_IMPL_CSS_COMPUTED
#ifndef MULIB_IMPL_CSS_COMPUTED_ONCE
#define MULIB_IMPL_CSS_COMPUTED_ONCE
#include "detail/mulib-css-declarations.h"
#include <string.h>

int
mulib_css_computed_defaults (mulib_css_computed_t *s)
{
  if (!s)
    return 0;
  memset (s, 0, sizeof *s);
  strcpy (s->display, "block");
  strcpy (s->color, "black");
  s->font_size_px = 16;
  s->width_px = -1;
  s->height_px = -1;
  return 1;
}
int
mulib_css_computed_apply (mulib_css_computed_t *s, const char *d, size_t n)
{
  if (!s)
    return 0;
  mulib_css_computed_t result = *s;
  int ranks[MULIB_CSS_DECL_COUNT_] = { 0 };
  if (!mulib_css_declarations_apply_ (&result, ranks, 0, d, n))
    return 0;
  *s = result;
  return 1;
}
#endif
#endif
