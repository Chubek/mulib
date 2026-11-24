/** @file mulib-html-tokenizer.h
 * @brief Length-aware HTML lexical tokens with tag-name and attribute spans.
 * C11 single-header module. Define MULIB_IMPL_HTML_TOKENIZER in exactly one C
 * translation unit. This is a practical lexical subset, not the full WHATWG
 * HTML5 tokenizer.
 */
#ifndef MULIB_HTML_TOKENIZER_H
#define MULIB_HTML_TOKENIZER_H
#include "mulib-html-textstream.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct mulib_html_token
  {
    mulib_html_textstream_kind_t kind;
    char name[64];
    const char *raw;
    size_t length, offset;
  } mulib_html_token_t;
  typedef int (*mulib_html_tokenizer_fn) (void *ctx,
                                          const mulib_html_token_t *t);
  /** @brief See manual/mulib-html-tokenizer.md for this operation's contract.
   */
  int mulib_html_tokenizer_scan (const char *html, size_t n,
                                 mulib_html_tokenizer_fn cb, void *ctx,
                                 size_t max_events);
  /** @brief Borrow the first exact, ASCII-case-insensitive attribute value. */
  int mulib_html_attribute (const char *tag, size_t n, const char *name,
                            const char **value, size_t *length);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_HTML_TOKENIZER_H */
#ifdef MULIB_IMPL_HTML_TOKENIZER
#ifndef MULIB_IMPL_HTML_TOKENIZER_ONCE
#define MULIB_IMPL_HTML_TOKENIZER_ONCE
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct mulib_html_tokenizer_ctx_
{
  mulib_html_tokenizer_fn cb;
  void *ctx;
} mulib_html_tokenizer_ctx_;
int
mulib_html_attribute (const char *s, size_t n, const char *name,
                      const char **value, size_t *length)
{
  if (!s || !name || !*name || !value || !length || n < 3 || s[0] != '<')
    return 0;
  size_t i = 1, k = strlen (name);
  while (i < n && !isspace ((unsigned char)s[i]) && s[i] != '>')
    i++;
  while (i < n)
    {
      while (i < n && isspace ((unsigned char)s[i]))
        i++;
      if (i == n || s[i] == '>' || s[i] == '/')
        break;
      size_t a = i;
      while (i < n && !isspace ((unsigned char)s[i]) && s[i] != '='
             && s[i] != '>' && s[i] != '/')
        i++;
      if (i == a)
        return 0;
      size_t alen = i - a;
      while (i < n && isspace ((unsigned char)s[i]))
        i++;
      size_t v = i, len = 0;
      if (i < n && s[i] == '=')
        {
          i++;
          while (i < n && isspace ((unsigned char)s[i]))
            i++;
          if (i == n)
            return 0;
          char quote = s[i] == '\'' || s[i] == '"' ? s[i++] : 0;
          v = i;
          while (i < n
                 && (quote ? s[i] != quote
                           : !isspace ((unsigned char)s[i]) && s[i] != '>'))
            i++;
          len = i - v;
          if (quote)
            {
              if (i == n)
                return 0;
              i++;
            }
        }
      int equal = alen == k;
      for (size_t j = 0; equal && j < k; j++)
        {
          unsigned char c = (unsigned char)s[a + j],
                        d = (unsigned char)name[j];
          if (c >= 'A' && c <= 'Z')
            c += 32;
          if (d >= 'A' && d <= 'Z')
            d += 32;
          if (c != d)
            equal = 0;
        }
      if (equal)
        {
          *value = s + v;
          *length = len;
          return 1;
        }
    }
  return 0;
}
static int
mulib_html_tokenizer_cb_ (void *p, const mulib_html_textstream_event_t *e)
{
  mulib_html_tokenizer_ctx_ *c = p;
  mulib_html_token_t t = { e->kind, { 0 }, e->start, e->len, e->offset };
  if (e->kind == MULIB_HTML_STREAM_START || e->kind == MULIB_HTML_STREAM_END)
    {
      size_t i = e->kind == MULIB_HTML_STREAM_END ? 2 : 1, k = 0;
      while (i < e->len && isspace ((unsigned char)e->start[i]))
        i++;
      while (i < e->len
             && (isalnum ((unsigned char)e->start[i]) || e->start[i] == '-'
                 || e->start[i] == ':'))
        {
          if (k + 1 >= sizeof t.name)
            return 0;
          t.name[k++] = (char)tolower ((unsigned char)e->start[i++]);
        }
    }
  return c->cb (c->ctx, &t);
}
int
mulib_html_tokenizer_scan (const char *s, size_t n, mulib_html_tokenizer_fn cb,
                           void *ctx, size_t max)
{
  if (!cb)
    return 0;
  mulib_html_tokenizer_ctx_ c = { cb, ctx };
  return mulib_html_textstream_scan (s, n, mulib_html_tokenizer_cb_, &c, max);
}
#endif
#endif
