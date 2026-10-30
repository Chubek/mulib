/** @file mulib-objfilepacker.h
 * @brief Deterministic short-name Unix ar member writer (no symbol index).
 * Define MULIB_IMPL_OBJFILEPACKER plus BUFFER. */
#ifndef MULIB_OBJFILEPACKER_H
#define MULIB_OBJFILEPACKER_H
#include "mulib-buffer.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Append a member, creating magic if buffer is empty. Names
   * are 1..15 ASCII alnum/._- bytes; no long names or special index members.
   * Content is opaque. 1 success, 0 invalid/overflow/OOM. Input may alias
   * archive storage. */
  int mulib_objfilepacker_add (mulib_buffer_t *archive, const char *name,
                               const void *data, size_t size);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_OBJFILEPACKER)                                         \
    && !defined(MULIB_OBJFILEPACKER_IMPLEMENTED)
#define MULIB_OBJFILEPACKER_IMPLEMENTED
#include <stdio.h>
#include <string.h>
int
mulib_objfilepacker_add (mulib_buffer_t *out, const char *name, const void *p,
                         size_t n)
{
  mulib_buffer_t b = { 0 };
  char h[60], num[32];
  size_t i, len;
  int used, ok = 0;
  if (!out || !name || (!p && n) || (uintmax_t)n > UINT64_C (9999999999))
    return 0;
  len = strlen (name);
  if (!len || len > 15 || !strcmp (name, ".") || !strcmp (name, ".."))
    return 0;
  for (i = 0; i < len; ++i)
    {
      unsigned char c = (unsigned char)name[i];
      if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
            || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-'))
        return 0;
    }
  if (out->length
      && (out->length < 8 || memcmp (out->data, "!<arch>\n", 8)
          || (out->length & 1)))
    return 0;
  memset (h, ' ', 60);
  memcpy (h, name, len);
  h[len] = '/';
  h[16] = '0';
  h[28] = '0';
  h[34] = '0';
  memcpy (h + 40, "100644", 6);
  used = snprintf (num, sizeof (num), "%llu", (unsigned long long)n);
  if (used < 1 || used > 10)
    return 0;
  memcpy (h + 48, num, (size_t)used);
  h[58] = '`';
  h[59] = '\n';
  if (!out->length && !mulib_buffer_append (&b, "!<arch>\n", 8))
    goto done;
  if (!mulib_buffer_append (&b, h, 60) || !mulib_buffer_append (&b, p, n)
      || ((n & 1) && !mulib_buffer_append (&b, "\n", 1)))
    goto done;
  ok = mulib_buffer_append (out, b.data, b.length);
done:
  mulib_buffer_free (&b);
  return ok;
}
#endif
