/** @file mulib-css-cascade.h
 * @brief Deterministic selected-property cascade with basic specificity and
 * source ordering. C11 single-header module. Define MULIB_IMPL_CSS_CASCADE in
 * exactly one C translation unit. Supports a deliberately small cascade
 * subset, without CSS layers or full inheritance.
 */
#ifndef MULIB_CSS_CASCADE_H
#define MULIB_CSS_CASCADE_H
#include "mulib-css-computed.h"
#include "mulib-css-parser.h"
#include "mulib-css-selector.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /** @brief See manual/mulib-css-cascade.md for this operation's contract. */
  int mulib_css_cascade_compute (const mulib_css_parsed_sheet_t *sheet,
                                 const mulib_html_tree_node_t *node,
                                 const mulib_css_computed_t *parent,
                                 mulib_css_computed_t *out);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_CSS_CASCADE_H */
#ifdef MULIB_IMPL_CSS_CASCADE
#ifndef MULIB_IMPL_CSS_CASCADE_ONCE
#define MULIB_IMPL_CSS_CASCADE_ONCE
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
mulib_css_cascade_compute (const mulib_css_parsed_sheet_t *sheet,
                           const mulib_html_tree_node_t *node,
                           const mulib_css_computed_t *parent,
                           mulib_css_computed_t *out)
{
  if (!sheet || !node || node->is_text || !out)
    return 0;
  mulib_css_computed_t result;
  if (!mulib_css_computed_defaults (&result))
    return 0;
  if (parent)
    {
      if (!isfinite (parent->font_size_px) || parent->font_size_px < 0
          || !memchr (parent->color, 0, sizeof parent->color))
        return 0;
      result.font_size_px = parent->font_size_px;
      memcpy (result.color, parent->color, sizeof result.color);
    }
  for (size_t i = 0; i < sheet->len; i++)
    if (!memchr (sheet->rules[i].selector, 0, sizeof sheet->rules[i].selector)
        || mulib_css_selector_specificity (sheet->rules[i].selector) < 0)
      return 0;
  for (int specificity = 0; specificity <= 100;
       specificity += specificity == 0   ? 1
                      : specificity == 1 ? 9
                                         : 90)
    {
      for (size_t i = 0; i < sheet->len; i++)
        {
          int spec = mulib_css_selector_specificity (sheet->rules[i].selector);
          if (spec != specificity
              || !mulib_css_selector_matches (node, sheet->rules[i].selector))
            continue;
          if (!mulib_css_computed_apply (&result,
                                         sheet->rules[i].declarations.data,
                                         sheet->rules[i].declarations.len))
            return 0;
        }
    }
  *out = result;
  return 1;
}
#endif
#endif
