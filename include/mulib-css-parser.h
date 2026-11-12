/** @file mulib-css-parser.h
 * @brief Owned bounded CSS stylesheet rules with selector/declaration spans.
 * C11 single-header module. Define MULIB_IMPL_CSS_PARSER in exactly one C
 * translation unit.
 *
 */
#ifndef MULIB_CSS_PARSER_H
#define MULIB_CSS_PARSER_H
#include "mulib-shylock-core.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct mulib_css_rule
  {
    char selector[256];
    mulib_shylock_buffer_t declarations;
  } mulib_css_rule_t;
  typedef struct mulib_css_parsed_sheet
  {
    mulib_css_rule_t *rules;
    size_t len, cap;
  } mulib_css_parsed_sheet_t;
  /** @brief See manual/mulib-css-parser.md for this operation's contract. */
  int mulib_css_parse_sheet (const char *source, size_t n, size_t max_rules,
                             mulib_css_parsed_sheet_t *out);
  /** @brief See manual/mulib-css-parser.md for this operation's contract. */
  void mulib_css_sheet_free (mulib_css_parsed_sheet_t *sheet);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_CSS_PARSER_H */
#ifdef MULIB_IMPL_CSS_PARSER
#ifndef MULIB_IMPL_CSS_PARSER_ONCE
#define MULIB_IMPL_CSS_PARSER_ONCE
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
mulib_css_trim_ (const char **p, size_t *n)
{
  while (*n && isspace ((unsigned char)**p))
    {
      (*p)++;
      (*n)--;
    }
  while (*n && isspace ((unsigned char)(*p)[*n - 1]))
    (*n)--;
}
int
mulib_css_parse_sheet (const char *src, size_t n, size_t max,
                       mulib_css_parsed_sheet_t *out)
{
  if ((!src && n) || !out || !max || n > 32 * 1024 * 1024
      || (n && memchr (src, 0, n)))
    return 0;
  mulib_shylock_buffer_t clean = { 0 };
  clean.limit = 32 * 1024 * 1024;
  mulib_css_parsed_sheet_t s = { 0 };
  char quote = 0;
  for (size_t i = 0; i < n; i++)
    {
      if (!quote && i + 1 < n && src[i] == '/' && src[i + 1] == '*')
        {
          i += 2;
          while (i + 1 < n && (src[i] != '*' || src[i + 1] != '/'))
            i++;
          if (i + 1 >= n)
            goto fail;
          i++;
          if (!mulib_shylock_text (&clean, " "))
            goto fail;
          continue;
        }
      char c = src[i];
      if (!mulib_shylock_append (&clean, &c, 1))
        goto fail;
      if (quote && c == '\\')
        {
          if (++i == n || !mulib_shylock_append (&clean, src + i, 1))
            goto fail;
        }
      else if (quote)
        {
          if (c == quote)
            quote = 0;
          else if (c == '\r' || c == '\n')
            goto fail;
        }
      else if (c == '"' || c == '\'')
        quote = c;
    }
  if (quote)
    goto fail;
  src = clean.data;
  n = clean.len;
  for (size_t i = 0; i < n;)
    {
      while (i < n && isspace ((unsigned char)src[i]))
        i++;
      if (i == n)
        break;
      size_t start = i;
      while (i < n && src[i] != '{' && src[i] != '}')
        i++;
      if (i == n || src[i] != '{')
        goto fail;
      const char *sel = src + start;
      size_t len = i - start;
      mulib_css_trim_ (&sel, &len);
      if (!len || len >= 256 || *sel == '@' || s.len >= max)
        goto fail;
      size_t body = ++i;
      quote = 0;
      unsigned parens = 0;
      for (; i < n; i++)
        {
          char c = src[i];
          if (quote)
            {
              if (c == '\\')
                {
                  if (++i == n)
                    goto fail;
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
                goto fail;
              parens--;
            }
          else if (c == '{')
            goto fail;
          else if (c == '}')
            {
              if (parens)
                goto fail;
              break;
            }
        }
      if (i == n || quote)
        goto fail;
      mulib_css_rule_t r = { 0 };
      memcpy (r.selector, sel, len);
      r.selector[len] = 0;
      if (!mulib_shylock_append (&r.declarations, src + body, i - body))
        goto fail;
      if (s.len == s.cap)
        {
          size_t cap = s.cap ? 2 * s.cap : 16;
          if (cap < s.cap || cap > SIZE_MAX / sizeof r)
            {
              mulib_shylock_buffer_free (&r.declarations);
              goto fail;
            }
          if (cap > max)
            cap = max;
          void *p = realloc (s.rules, cap * sizeof r);
          if (!p)
            {
              mulib_shylock_buffer_free (&r.declarations);
              goto fail;
            }
          s.rules = p;
          s.cap = cap;
        }
      s.rules[s.len++] = r;
      i++;
    }
  mulib_shylock_buffer_free (&clean);
  *out = s;
  return 1;
fail:
  mulib_shylock_buffer_free (&clean);
  mulib_css_sheet_free (&s);
  return 0;
}
void
mulib_css_sheet_free (mulib_css_parsed_sheet_t *s)
{
  if (s)
    {
      for (size_t i = 0; i < s->len; i++)
        mulib_shylock_buffer_free (&s->rules[i].declarations);
      free (s->rules);
      memset (s, 0, sizeof *s);
    }
}
#endif
#endif
