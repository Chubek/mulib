/** @file mulib-multipart.h
 * @brief Multipart/form-data boundary and field formatting for simple text
 * fields. C11 single-header implementation. Define MULIB_IMPL_MULTIPART in one
 * translation unit.
 */
#ifndef MULIB_MULTIPART_H
#define MULIB_MULTIPART_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Format one text field, excluding the closing multipart delimiter.
   * Boundary is 1..70 ASCII alphanumeric/hyphen/underscore bytes; name is
   * nonempty without controls, quotes or backslashes. Rejects boundary
   * delimiter lines in value. Return bytes excluding NUL, or 0; output
   * unchanged on error. No overlap. */
  size_t mulib_multipart_field (const char *boundary, const char *name,
                                const char *value, char *out, size_t cap);

#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_MULTIPART) && !defined(MULIB_MULTIPART_IMPLEMENTED)
#define MULIB_MULTIPART_IMPLEMENTED
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif

  size_t
  mulib_multipart_field (const char *b, const char *n, const char *v, char *o,
                         size_t cap)
  {
    if (!b || !n || !v || !o || !*b || !*n)
      return 0;
    size_t bn = strlen (b);
    if (bn > 70)
      return 0;
    for (const char *p = b; *p; ++p)
      {
        unsigned char c = (unsigned char)*p;
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
              || (c >= '0' && c <= '9') || c == '-' || c == '_'))
          return 0;
      }
    for (const char *p = n; *p; ++p)
      if ((unsigned char)*p < 32 || (unsigned char)*p == 127 || *p == '"'
          || *p == '\\')
        return 0;
    for (const char *p = v; *p; ++p)
      if ((p == v || p[-1] == '\n') && !strncmp (p, "--", 2)
          && !strncmp (p + 2, b, bn))
        return 0;
    const char *fmt
        = "--%s\r\nContent-Disposition: form-data; name=\"%s\"\r\n\r\n%s\r\n";
    int z = snprintf (NULL, 0, fmt, b, n, v);
    if (z < 0 || (size_t)z >= cap)
      return 0;
    return snprintf (o, cap, fmt, b, n, v) == z ? (size_t)z : 0;
  }

#ifdef __cplusplus
}
#endif
#endif
