/** @file mulib-webidl-bindgen.h
 * @brief C header declaration generation from a bounded interface-name WebIDL
 * subset. C11 single-header module. Define MULIB_IMPL_WEBIDL_BINDGEN in
 * exactly one C translation unit. Generates declaration handles, not full
 * WebIDL binding semantics.
 */
#ifndef MULIB_WEBIDL_BINDGEN_H
#define MULIB_WEBIDL_BINDGEN_H
#include "mulib-shylock-core.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /** @brief See manual/mulib-webidl-bindgen.md for this operation's contract.
   */
  int mulib_webidl_bindgen_interfaces (const char *idl, size_t n,
                                       mulib_shylock_buffer_t *out,
                                       size_t max_interfaces);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_WEBIDL_BINDGEN_H */
#ifdef MULIB_IMPL_WEBIDL_BINDGEN
#ifndef MULIB_IMPL_WEBIDL_BINDGEN_ONCE
#define MULIB_IMPL_WEBIDL_BINDGEN_ONCE
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int
mulib_webidl_skip_ (const char *s, size_t n, size_t *i)
{
  for (;;)
    {
      while (*i < n && isspace ((unsigned char)s[*i]))
        (*i)++;
      if (n - *i >= 2 && s[*i] == '/' && s[*i + 1] == '/')
        {
          *i += 2;
          while (*i < n && s[*i] != '\n')
            (*i)++;
        }
      else if (n - *i >= 2 && s[*i] == '/' && s[*i + 1] == '*')
        {
          *i += 2;
          while (n - *i >= 2 && (s[*i] != '*' || s[*i + 1] != '/'))
            (*i)++;
          if (n - *i < 2)
            return 0;
          *i += 2;
        }
      else
        return 1;
    }
}
int
mulib_webidl_bindgen_interfaces (const char *s, size_t n,
                                 mulib_shylock_buffer_t *out, size_t limit)
{
  if ((!s && n) || !out || n > 4 * 1024 * 1024 || (n && memchr (s, 0, n)))
    return 0;
  mulib_shylock_buffer_t b = { 0 };
  b.limit = out->limit;
  int ok = 0;
  size_t count = 0, i = 0;
  if (!mulib_shylock_text (&b, "/* Generated C interface handles. */\n"))
    goto done;
  while (i < n)
    {
      if (!mulib_webidl_skip_ (s, n, &i))
        goto done;
      if (i == n)
        break;
      if (n - i < 10 || memcmp (s + i, "interface", 9)
          || !isspace ((unsigned char)s[i + 9]))
        goto done;
      i += 9;
      if (!mulib_webidl_skip_ (s, n, &i) || i == n
          || (!isalpha ((unsigned char)s[i]) && s[i] != '_'))
        goto done;
      size_t start = i++;
      while (i < n && (isalnum ((unsigned char)s[i]) || s[i] == '_'))
        i++;
      size_t len = i - start;
      if (len > 80 || count >= limit || !mulib_webidl_skip_ (s, n, &i)
          || i == n)
        goto done;
      if (s[i] == '{')
        {
          size_t depth = 1;
          i++;
          while (i < n && depth)
            {
              if (!mulib_webidl_skip_ (s, n, &i) || i == n)
                goto done;
              char c = s[i++];
              if (c == '{')
                {
                  if (++depth > 64)
                    goto done;
                }
              else if (c == '}')
                depth--;
              else if (c == '"' || c == '\'')
                {
                  char q = c;
                  while (i < n && s[i] != q)
                    {
                      if (s[i] == '\\')
                        i++;
                      if (i == n)
                        goto done;
                      i++;
                    }
                  if (i == n)
                    goto done;
                  i++;
                }
            }
          if (depth || !mulib_webidl_skip_ (s, n, &i))
            goto done;
        }
      if (i == n || s[i++] != ';')
        goto done;
      if (!mulib_shylock_text (&b, "typedef struct mulib_webidl_")
          || !mulib_shylock_append (&b, s + start, len)
          || !mulib_shylock_text (&b, " mulib_webidl_")
          || !mulib_shylock_append (&b, s + start, len)
          || !mulib_shylock_text (&b, ";\n"))
        goto done;
      count++;
    }
  ok = mulib_shylock_append (out, b.data, b.len);
done:
  mulib_shylock_buffer_free (&b);
  return ok;
}
#endif
#endif
