/** @file mulib-css-computed.h
 * @brief Selected CSS declaration lookup with inherited color and font-size
 * fields. C11 single-header module. Define MULIB_IMPL_CSS_COMPUTED in exactly
 * one C translation unit.
 *
 */
#ifndef MULIB_CSS_COMPUTED_H
#define MULIB_CSS_COMPUTED_H
#include "mulib-shylock-core.h"
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
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
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
  if (!s || (!d && n) || (n && memchr (d, 0, n)))
    return 0;
  mulib_css_computed_t result = *s;
  for (size_t i = 0; i < n;)
    {
      while (i < n && (isspace ((unsigned char)d[i]) || d[i] == ';'))
        i++;
      if (i == n)
        break;
      size_t start = i;
      while (i < n && d[i] != ':' && d[i] != ';')
        i++;
      if (i == n || d[i] != ':')
        return 0;
      size_t end = i++;
      while (end > start && isspace ((unsigned char)d[end - 1]))
        end--;
      char key[64];
      if (end - start >= sizeof key || end == start)
        return 0;
      for (size_t j = 0; j < end - start; j++)
        key[j] = (char)tolower ((unsigned char)d[start + j]);
      key[end - start] = 0;
      while (i < n && isspace ((unsigned char)d[i]))
        i++;
      size_t from = i;
      char quote = 0;
      unsigned parens = 0;
      for (; i < n; i++)
        {
          char c = d[i];
          if (quote)
            {
              if (c == '\\')
                {
                  if (++i == n)
                    return 0;
                }
              else if (c == quote)
                quote = 0;
            }
          else if (c == '"' || c == '\'')
            quote = c;
          else if (c == '(')
            parens++;
          else if (c == ')')
            {
              if (!parens)
                return 0;
              parens--;
            }
          else if (c == ';' && !parens)
            break;
        }
      if (quote || parens)
        return 0;
      size_t last = i;
      while (last > from && isspace ((unsigned char)d[last - 1]))
        last--;
      if (last == from)
        return 0;
      int display = !strcmp (key, "display"), color = !strcmp (key, "color"),
          width = !strcmp (key, "width"), height = !strcmp (key, "height"),
          font = !strcmp (key, "font-size");
      if (!display && !color && !width && !height && !font)
        continue;
      size_t size = last - from;
      char value[64];
      if (size >= sizeof value)
        return 0;
      memcpy (value, d + from, size);
      value[size] = 0;
      if (strchr (value, '!'))
        return 0;
      if (display)
        {
          if (strcmp (value, "block") && strcmp (value, "inline")
              && strcmp (value, "none") && strcmp (value, "flex")
              && strcmp (value, "grid"))
            return 0;
          mulib_shylock_copy (result.display, sizeof result.display, value);
        }
      else if (color)
        mulib_shylock_copy (result.color, sizeof result.color, value);
      else
        {
          double x;
          if (!font && !strcmp (value, "auto"))
            x = -1;
          else
            {
              if (size >= 2 && !strcmp (value + size - 2, "px"))
                value[size - 2] = 0;
              if (!mulib_shylock_parse_decimal (value, &x) || x < 0)
                return 0;
            }
          if (width)
            result.width_px = x;
          else if (height)
            result.height_px = x;
          else
            result.font_size_px = x;
        }
    }
  *s = result;
  return 1;
}
#endif
#endif
