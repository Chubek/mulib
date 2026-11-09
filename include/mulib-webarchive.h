/** @file mulib-webarchive.h
 * @brief Write WARC-like minimal record headers into a bounded caller buffer.
 * C11 single-header implementation. Define MULIB_IMPL_WEBARCHIVE in one
 * translation unit.
 */
#ifndef MULIB_WEBARCHIVE_H
#define MULIB_WEBARCHIVE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Write a WARC-like resource header; NOT a conforming WARC record
   * (no date/record ID). Nonempty printable ASCII URI without whitespace only.
   * Returns bytes excluding NUL, or 0 unchanged on error. No overlap. */
  size_t mulib_webarchive_record_header (const char *url, size_t body_bytes,
                                         char *out, size_t cap);

#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_WEBARCHIVE) && !defined(MULIB_WEBARCHIVE_IMPLEMENTED)
#define MULIB_WEBARCHIVE_IMPLEMENTED
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
  mulib_webarchive_record_header (const char *url, size_t n, char *o,
                                  size_t cap)
  {
    if (!url || !o || !*url)
      return 0;
    for (const unsigned char *p = (const unsigned char *)url; *p; ++p)
      if (*p <= 32 || *p >= 127)
        return 0;
    const char *fmt = "WARC/1.0\r\nWARC-Type: resource\r\nWARC-Target-URI: "
                      "%s\r\nContent-Length: %zu\r\n\r\n";
    int z = snprintf (NULL, 0, fmt, url, n);
    if (z < 0 || (size_t)z >= cap)
      return 0;
    return snprintf (o, cap, fmt, url, n) == z ? (size_t)z : 0;
  }

#ifdef __cplusplus
}
#endif
#endif
