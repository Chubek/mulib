/* Implementation detail shared by css-computed and css-cascade.
 * Both includers have already declared css-computed and bounded-text. */
#ifndef MULIB_CSS_DECLARATIONS_DETAIL_H
#define MULIB_CSS_DECLARATIONS_DETAIL_H

#include <string.h>

enum
{
  MULIB_CSS_DECL_DISPLAY_,
  MULIB_CSS_DECL_COLOR_,
  MULIB_CSS_DECL_WIDTH_,
  MULIB_CSS_DECL_HEIGHT_,
  MULIB_CSS_DECL_FONT_SIZE_,
  MULIB_CSS_DECL_COUNT_
};

static int
mulib_css_decl_space_ (char c)
{
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f';
}

static char
mulib_css_decl_lower_ (char c)
{
  return c >= 'A' && c <= 'Z' ? (char)(c + ('a' - 'A')) : c;
}

static int
mulib_css_decl_important_ (const char *s, size_t n)
{
  const char keyword[] = "important";
  while (n && mulib_css_decl_space_ (*s))
    {
      s++;
      n--;
    }
  if (n != sizeof keyword - 1)
    return 0;
  for (size_t i = 0; i < n; i++)
    if (mulib_css_decl_lower_ (s[i]) != keyword[i])
      return 0;
  return 1;
}

/* Modify a caller-local candidate only. The caller publishes it on success.
 * Ranks are per property; equal rank lets the later declaration win. */
static int
mulib_css_declarations_apply_ (mulib_css_computed_t *result, int *ranks,
                              int specificity, const char *d, size_t n)
{
  static const char *const keys[MULIB_CSS_DECL_COUNT_]
      = { "display", "color", "width", "height", "font-size" };
  if ((!d && n) || (n && memchr (d, 0, n)))
    return 0;
  for (size_t i = 0; i < n;)
    {
      while (i < n && (mulib_css_decl_space_ (d[i]) || d[i] == ';'))
        i++;
      if (i == n)
        break;
      size_t start = i;
      while (i < n && d[i] != ':' && d[i] != ';')
        i++;
      if (i == n || d[i] != ':')
        return 0;
      size_t end = i++;
      while (end > start && mulib_css_decl_space_ (d[end - 1]))
        end--;
      char key[64];
      if (end - start >= sizeof key || end == start)
        return 0;
      for (size_t j = 0; j < end - start; j++)
        key[j] = mulib_css_decl_lower_ (d[start + j]);
      key[end - start] = 0;
      int property = 0;
      while (property < MULIB_CSS_DECL_COUNT_
             && strcmp (key, keys[property]))
        property++;

      while (i < n && mulib_css_decl_space_ (d[i]))
        i++;
      size_t from = i, bang = n, parens = 0;
      char quote = 0;
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
          else if (!parens)
            {
              if (c == ';')
                break;
              if (c == '!' && bang == n)
                bang = i;
            }
        }
      if (quote || parens)
        return 0;
      size_t last = i;
      while (last > from && mulib_css_decl_space_ (d[last - 1]))
        last--;
      if (last == from)
        return 0;
      if (property == MULIB_CSS_DECL_COUNT_)
        continue;

      int rank = specificity;
      if (bang != n)
        {
          if (!mulib_css_decl_important_ (d + bang + 1, last - bang - 1))
            return 0;
          /* The simple-selector specificity is at most 100. */
          rank += 101;
          last = bang;
          while (last > from && mulib_css_decl_space_ (d[last - 1]))
            last--;
        }
      size_t size = last - from;
      char value[64];
      if (!size || size >= sizeof value)
        return 0;
      memcpy (value, d + from, size);
      value[size] = 0;
      int wins = rank >= ranks[property];
      if (property == MULIB_CSS_DECL_DISPLAY_)
        {
          if (strcmp (value, "block") && strcmp (value, "inline")
              && strcmp (value, "none") && strcmp (value, "flex")
              && strcmp (value, "grid"))
            return 0;
          if (wins)
            mulib_bounded_text_copy (result->display, sizeof result->display,
                                     value);
        }
      else if (property == MULIB_CSS_DECL_COLOR_)
        {
          if (wins)
            mulib_bounded_text_copy (result->color, sizeof result->color,
                                     value);
        }
      else
        {
          double x;
          if (property != MULIB_CSS_DECL_FONT_SIZE_ && !strcmp (value, "auto"))
            x = -1;
          else
            {
              if (size >= 2 && !strcmp (value + size - 2, "px"))
                value[size - 2] = 0;
              if (!mulib_bounded_text_parse_decimal (value, &x) || x < 0)
                return 0;
            }
          if (wins)
            {
              if (property == MULIB_CSS_DECL_WIDTH_)
                result->width_px = x;
              else if (property == MULIB_CSS_DECL_HEIGHT_)
                result->height_px = x;
              else
                result->font_size_px = x;
            }
        }
      if (wins)
        ranks[property] = rank;
    }
  return 1;
}

#endif /* MULIB_CSS_DECLARATIONS_DETAIL_H */
