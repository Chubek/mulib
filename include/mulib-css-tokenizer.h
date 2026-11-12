/** @file mulib-css-tokenizer.h
 * @brief Length-aware CSS identifiers, numbers, strings and symbols; skips
 * comments. C11 single-header module. Define MULIB_IMPL_CSS_TOKENIZER in
 * exactly one C translation unit. Tokenizes a restricted CSS profile; not a
 * complete CSS Syntax Level 3 tokenizer.
 */
#ifndef MULIB_CSS_TOKENIZER_H
#define MULIB_CSS_TOKENIZER_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef enum mulib_css_token_kind
  {
    MULIB_CSS_TOKEN_IDENT = 1,
    MULIB_CSS_TOKEN_STRING = 2,
    MULIB_CSS_TOKEN_NUMBER = 3,
    MULIB_CSS_TOKEN_SYMBOL = 4
  } mulib_css_token_kind_t;
  typedef struct mulib_css_token
  {
    mulib_css_token_kind_t kind;
    const char *bytes;
    size_t length, offset;
  } mulib_css_token_t;
  typedef int (*mulib_css_tokenizer_fn) (void *user,
                                         const mulib_css_token_t *token);
  /** @brief See manual/mulib-css-tokenizer.md for this operation's contract.
   */
  int mulib_css_tokenizer_scan (const char *source, size_t n,
                                mulib_css_tokenizer_fn fn, void *ctx,
                                size_t limit);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_CSS_TOKENIZER_H */
#ifdef MULIB_IMPL_CSS_TOKENIZER
#ifndef MULIB_IMPL_CSS_TOKENIZER_ONCE
#define MULIB_IMPL_CSS_TOKENIZER_ONCE
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
mulib_css_tokenizer_scan (const char *s, size_t n, mulib_css_tokenizer_fn fn,
                          void *ctx, size_t limit)
{
  if ((!s && n) || !fn || (n && memchr (s, 0, n)))
    return 0;
  size_t count = 0;
  for (size_t i = 0; i < n;)
    {
      if (isspace ((unsigned char)s[i]))
        {
          i++;
          continue;
        }
      if (i + 1 < n && s[i] == '/' && s[i + 1] == '*')
        {
          i += 2;
          while (i + 1 < n && (s[i] != '*' || s[i + 1] != '/'))
            i++;
          if (i + 1 >= n)
            return 0;
          i += 2;
          continue;
        }
      size_t k = i;
      mulib_css_token_kind_t kind = MULIB_CSS_TOKEN_SYMBOL;
      size_t number = i;
      if (s[number] == '+' || s[number] == '-')
        number++;
      int numeric = number < n
                    && (isdigit ((unsigned char)s[number])
                        || (s[number] == '.' && number + 1 < n
                            && isdigit ((unsigned char)s[number + 1])));
      if (numeric)
        {
          kind = MULIB_CSS_TOKEN_NUMBER;
          i = number;
          while (i < n && isdigit ((unsigned char)s[i]))
            i++;
          if (i + 1 < n && s[i] == '.' && isdigit ((unsigned char)s[i + 1]))
            {
              i++;
              while (i < n && isdigit ((unsigned char)s[i]))
                i++;
            }
          if (i < n && (s[i] == 'e' || s[i] == 'E'))
            {
              size_t e = i + 1;
              if (e < n && (s[e] == '+' || s[e] == '-'))
                e++;
              if (e < n && isdigit ((unsigned char)s[e]))
                {
                  i = e + 1;
                  while (i < n && isdigit ((unsigned char)s[i]))
                    i++;
                }
            }
        }
      else if (isalpha ((unsigned char)s[i]) || s[i] == '_' || s[i] == '-')
        {
          kind = MULIB_CSS_TOKEN_IDENT;
          while (
              i < n
              && (isalnum ((unsigned char)s[i]) || s[i] == '_' || s[i] == '-'))
            i++;
        }
      else if (s[i] == '"' || s[i] == '\'')
        {
          kind = MULIB_CSS_TOKEN_STRING;
          char q = s[i++];
          while (i < n && s[i] != q)
            {
              if (s[i] == '\\' && i + 1 < n)
                i++;
              i++;
            }
          if (i == n)
            return 0;
          i++;
        }
      else
        i++;
      if (limit && count >= limit)
        return 0;
      count++;
      mulib_css_token_t t = { kind, s + k, i - k, k };
      if (!fn (ctx, &t))
        return 0;
    }
  return 1;
}
#endif
#endif
