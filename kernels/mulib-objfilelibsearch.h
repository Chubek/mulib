/** @file mulib-objfilelibsearch.h
 * @brief Checked short-name Unix ar lookup over borrowed bytes.
 * Define MULIB_IMPL_OBJFILELIBSEARCH once. Long-name and thin archives
 * rejected. */
#ifndef MULIB_OBJFILELIBSEARCH_H
#define MULIB_OBJFILELIBSEARCH_H
#include "mulib-binfile.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Validate every member before returning first matching member. 0
   * found, 1 absent, -1 malformed/unsupported/invalid. Output unchanged on
   * failure. */
  int mulib_objfilelibsearch_find (const void *archive, size_t size,
                                   const char *name, mulib_binfile_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_OBJFILELIBSEARCH)                                      \
    && !defined(MULIB_OBJFILELIBSEARCH_IMPLEMENTED)
#define MULIB_OBJFILELIBSEARCH_IMPLEMENTED
#include <stdint.h>
#include <string.h>
int
mulib_objfilelibsearch_find (const void *data, size_t n, const char *name,
                             mulib_binfile_t *out)
{
  const unsigned char *p = (const unsigned char *)data;
  size_t off = 8, len, i, sz, end;
  int found = 0;
  mulib_binfile_t result = { NULL, 0 };
  if (!p || !name || !out || n < 8 || memcmp (p, "!<arch>\n", 8))
    return -1;
  len = strlen (name);
  while (off < n)
    {
      size_t namelen = 0;
      int digits = 0, space = 0;
      if (n - off < 60 || p[off + 58] != '`' || p[off + 59] != '\n')
        return -1;
      while (namelen < 16 && p[off + namelen] != '/'
             && p[off + namelen] != ' ')
        ++namelen;
      if (!namelen || namelen >= 16 || p[off + namelen] != '/')
        return -1;
      if ((namelen == 1 && p[off] == '.')
          || (namelen == 2 && p[off] == '.' && p[off + 1] == '.'))
        return -1;
      for (i = 0; i < namelen; ++i)
        {
          unsigned char c = p[off + i];
          if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
                || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-'))
            return -1;
        }
      for (i = namelen + 1; i < 16; ++i)
        if (p[off + i] != ' ')
          return -1;
      sz = 0;
      for (i = 48; i < 58; ++i)
        {
          unsigned char c = p[off + i];
          if (c == ' ')
            {
              space = 1;
              continue;
            }
          if (space || c < '0' || c > '9' || sz > (SIZE_MAX - (c - '0')) / 10)
            return -1;
          sz = sz * 10 + (size_t)(c - '0');
          digits = 1;
        }
      if (!digits || sz > n - off - 60)
        return -1;
      end = off + 60 + sz;
      if ((sz & 1) && (end == n || p[end] != '\n'))
        return -1;
      if (!found && namelen == len && !memcmp (p + off, name, len))
        {
          result.data = p + off + 60;
          result.size = sz;
          found = 1;
        }
      off = end + (sz & 1);
    }
  if (found)
    {
      *out = result;
      return 0;
    }
  return 1;
}
#endif
