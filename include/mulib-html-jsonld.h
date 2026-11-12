/** @file mulib-html-jsonld.h
 * @brief Locate multiple application/ld+json script bodies as borrowed spans.
 * C11 single-header module. Define MULIB_IMPL_HTML_JSONLD in exactly one C
 * translation unit.
 *
 */
#ifndef MULIB_HTML_JSONLD_H
#define MULIB_HTML_JSONLD_H
#include "mulib-html-tokenizer.h"
#include "mulib-shylock-core.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef int (*mulib_html_jsonld_fn) (void *ctx, mulib_shylock_span_t source,
                                       size_t offset);
  /** @brief See manual/mulib-html-jsonld.md for this operation's contract. */
  int mulib_html_jsonld_scan (const char *html, size_t n,
                              mulib_html_jsonld_fn callback, void *ctx,
                              size_t limit);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_HTML_JSONLD_H */
#ifdef MULIB_IMPL_HTML_JSONLD
#ifndef MULIB_IMPL_HTML_JSONLD_ONCE
#define MULIB_IMPL_HTML_JSONLD_ONCE
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct mulib_html_jsonld_state_
{
  const char *source;
  mulib_html_jsonld_fn fn;
  void *ctx;
  size_t start, count, max;
  int active;
} mulib_html_jsonld_state_;
static int
mulib_html_jsonld_token_ (void *ctx, const mulib_html_token_t *t)
{
  mulib_html_jsonld_state_ *s = ctx;
  if (t->kind == MULIB_HTML_STREAM_START && !strcmp (t->name, "script"))
    {
      const char *v;
      size_t n;
      if (mulib_html_attribute (t->raw, t->length, "type", &v, &n))
        {
          while (n && isspace ((unsigned char)*v))
            {
              v++;
              n--;
            }
          while (n && isspace ((unsigned char)v[n - 1]))
            n--;
          if (n == 19)
            {
              char type[20];
              memcpy (type, v, n);
              type[n] = 0;
              s->active = mulib_shylock_equal_ci (type, "application/ld+json");
            }
        }
      if (s->active)
        s->start = t->offset + t->length;
    }
  else if (s->active && t->kind == MULIB_HTML_STREAM_END
           && !strcmp (t->name, "script"))
    {
      if (s->max && s->count >= s->max)
        return 0;
      mulib_shylock_span_t span
          = { s->source + s->start, t->offset - s->start };
      if (!s->fn (s->ctx, span, s->start))
        return 0;
      s->count++;
      s->active = 0;
    }
  return 1;
}
int
mulib_html_jsonld_scan (const char *s, size_t n, mulib_html_jsonld_fn fn,
                        void *ctx, size_t max)
{
  if ((!s && n) || !fn)
    return 0;
  mulib_html_jsonld_state_ st = { s, fn, ctx, 0, 0, max, 0 };
  return mulib_html_tokenizer_scan (s, n, mulib_html_jsonld_token_, &st,
                                    1000000)
         && !st.active;
}
#endif
#endif
