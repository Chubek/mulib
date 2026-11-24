/** @file mulib-html-textstream.h
 * @brief Length-aware tokenizer-style HTML tag/text/comment callbacks with
 * source offsets. C11 single-header module. Define MULIB_IMPL_HTML_TEXTSTREAM
 * in exactly one C translation unit.
 *
 */
#ifndef MULIB_HTML_TEXTSTREAM_H
#define MULIB_HTML_TEXTSTREAM_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef enum mulib_html_textstream_kind
  {
    MULIB_HTML_STREAM_START = 1,
    MULIB_HTML_STREAM_END = 2,
    MULIB_HTML_STREAM_TEXT = 3,
    MULIB_HTML_STREAM_COMMENT = 4
  } mulib_html_textstream_kind_t;
  typedef struct mulib_html_textstream_event
  {
    mulib_html_textstream_kind_t kind;
    const char *start;
    size_t len, offset;
  } mulib_html_textstream_event_t;
  typedef int (*mulib_html_textstream_fn) (
      void *user, const mulib_html_textstream_event_t *event);
  /** @brief See manual/mulib-html-textstream.md for this operation's contract.
   */
  int mulib_html_textstream_scan (const char *html, size_t length,
                                  mulib_html_textstream_fn callback, void *ctx,
                                  size_t max_events);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_HTML_TEXTSTREAM_H */
#ifdef MULIB_IMPL_HTML_TEXTSTREAM
#ifndef MULIB_IMPL_HTML_TEXTSTREAM_ONCE
#define MULIB_IMPL_HTML_TEXTSTREAM_ONCE
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int
mulib_html_textstream_ci_ (const char *s, const char *word, size_t n)
{
  for (size_t i = 0; i < n; i++)
    {
      unsigned char c = (unsigned char)s[i];
      if ((c >= 'A' && c <= 'Z' ? c + 32 : c) != (unsigned char)word[i])
        return 0;
    }
  return 1;
}
int
mulib_html_textstream_scan (const char *s, size_t n,
                            mulib_html_textstream_fn fn, void *ctx, size_t max)
{
  if ((!s && n) || !fn || (n && memchr (s, 0, n)))
    return 0;
  size_t i = 0, events = 0;
  char raw[16] = { 0 };
  while (i < n)
    {
      size_t start = i;
      mulib_html_textstream_kind_t kind = MULIB_HTML_STREAM_TEXT;
      if (raw[0])
        {
          size_t k = strlen (raw);
          while (i < n)
            {
              if (n - i >= k + 3 && s[i] == '<' && s[i + 1] == '/'
                  && mulib_html_textstream_ci_ (s + i + 2, raw, k)
                  && (s[i + k + 2] == '>'
                      || isspace ((unsigned char)s[i + k + 2])))
                break;
              i++;
            }
          if (i == n)
            return 0;
          raw[0] = 0;
          if (i == start)
            continue;
        }
      else if (s[i] == '<' && n - i >= 2
               && (isalpha ((unsigned char)s[i + 1]) || s[i + 1] == '/'
                   || s[i + 1] == '!'))
        {
          if (n - i >= 4 && !memcmp (s + i, "<!--", 4))
            {
              kind = MULIB_HTML_STREAM_COMMENT;
              i += 4;
              while (n - i >= 3 && memcmp (s + i, "-->", 3))
                i++;
              if (n - i < 3)
                return 0;
              i += 3;
            }
          else
            {
              kind = s[i + 1] == '!'   ? MULIB_HTML_STREAM_COMMENT
                     : s[i + 1] == '/' ? MULIB_HTML_STREAM_END
                                       : MULIB_HTML_STREAM_START;
              i++;
              char quote = 0;
              int closed = 0;
              for (; i < n; i++)
                {
                  if (quote)
                    {
                      if (s[i] == quote)
                        quote = 0;
                    }
                  else if (s[i] == '\'' || s[i] == '"')
                    quote = s[i];
                  else if (s[i] == '>')
                    {
                      i++;
                      closed = 1;
                      break;
                    }
                }
              if (!closed)
                return 0;
              if (kind == MULIB_HTML_STREAM_START)
                {
                  const char *tags[]
                      = { "script", "style", "textarea", "title" };
                  for (size_t t = 0; t < 4; t++)
                    {
                      size_t k = strlen (tags[t]);
                      if (i - start >= k + 2
                          && mulib_html_textstream_ci_ (s + start + 1, tags[t],
                                                        k)
                          && (s[start + k + 1] == '>'
                              || s[start + k + 1] == '/'
                              || isspace ((unsigned char)s[start + k + 1]))
                          && s[i - 2] != '/')
                        strcpy (raw, tags[t]);
                    }
                }
            }
        }
      else
        {
          while (i < n && s[i] != '<')
            i++;
          if (i == start)
            i++;
        }
      if (max && events >= max)
        return 0;
      mulib_html_textstream_event_t e = { kind, s + start, i - start, start };
      if (!fn (ctx, &e))
        return 0;
      events++;
    }
  return !raw[0];
}
#endif
#endif
