/** @file mulib-readability.h
 * @brief Bounded semantic plain-text extraction from HTML with basic entity
 * decoding. C11 single-header module. Define MULIB_IMPL_READABILITY in exactly
 * one C translation unit.
 *
 */
#ifndef MULIB_READABILITY_H
#define MULIB_READABILITY_H
#include "mulib-html-textstream.h"
#include "mulib-shylock-core.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /** @brief See manual/mulib-readability.md for this operation's contract. */
  int mulib_readability_extract (const char *html, size_t length,
                                 mulib_shylock_buffer_t *out);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_READABILITY_H */
#ifdef MULIB_IMPL_READABILITY
#ifndef MULIB_IMPL_READABILITY_ONCE
#define MULIB_IMPL_READABILITY_ONCE
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct mulib_readability_state_
{
  mulib_shylock_buffer_t *out;
  int skip, space;
} mulib_readability_state_;
static int
mulib_readability_hidden_ (const mulib_html_textstream_event_t *e)
{
  size_t i = e->kind == MULIB_HTML_STREAM_END ? 2 : 1, k = 0;
  char name[16] = { 0 };
  while (i < e->len && isalpha ((unsigned char)e->start[i])
         && k + 1 < sizeof name)
    name[k++] = (char)tolower ((unsigned char)e->start[i++]);
  return (!strcmp (name, "script") || !strcmp (name, "style")
          || !strcmp (name, "noscript"))
         && i < e->len
         && (e->start[i] == '>' || e->start[i] == '/'
             || isspace ((unsigned char)e->start[i]));
}
static int
mulib_readability_event_ (void *ctx, const mulib_html_textstream_event_t *e)
{
  mulib_readability_state_ *s = ctx;
  if (e->kind == MULIB_HTML_STREAM_START)
    {
      if (mulib_readability_hidden_ (e) && e->start[e->len - 2] != '/')
        s->skip++;
      s->space = 1;
      return 1;
    }
  if (e->kind == MULIB_HTML_STREAM_END)
    {
      if (mulib_readability_hidden_ (e) && s->skip)
        s->skip--;
      s->space = 1;
      return 1;
    }
  if (e->kind != MULIB_HTML_STREAM_TEXT || s->skip)
    return 1;
  for (size_t i = 0; i < e->len; i++)
    {
      unsigned char c = (unsigned char)e->start[i];
      if (c == '&')
        {
          const struct
          {
            const char *s;
            char c;
          } entities[]
              = { { "&amp;", '&' },  { "&lt;", '<' },    { "&gt;", '>' },
                  { "&quot;", '"' }, { "&apos;", '\'' }, { "&nbsp;", ' ' } };
          for (size_t k = 0; k < sizeof entities / sizeof *entities; k++)
            {
              size_t len = strlen (entities[k].s);
              if (i + len <= e->len
                  && !memcmp (e->start + i, entities[k].s, len))
                {
                  c = (unsigned char)entities[k].c;
                  i += len - 1;
                  break;
                }
            }
        }
      if (isspace (c))
        {
          s->space = 1;
          continue;
        }
      if (s->space && s->out->len)
        {
          if (!mulib_shylock_text (s->out, " "))
            return 0;
        }
      s->space = 0;
      if (!mulib_shylock_append (s->out, &c, 1))
        return 0;
    }
  return 1;
}
int
mulib_readability_extract (const char *s, size_t n,
                           mulib_shylock_buffer_t *out)
{
  if (!out)
    return 0;
  mulib_shylock_buffer_t tmp = { 0 };
  tmp.limit = out->limit;
  mulib_readability_state_ st = { &tmp, 0, 0 };
  int ok = mulib_html_textstream_scan (s, n, mulib_readability_event_, &st,
                                       1000000);
  if (ok)
    ok = mulib_shylock_append (out, tmp.data, tmp.len);
  mulib_shylock_buffer_free (&tmp);
  return ok;
}
#endif
#endif
