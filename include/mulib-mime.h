/** @file mulib-mime.h
 * @brief Case-insensitive filename-extension MIME lookup; no content sniffing.
 * Define MULIB_IMPL_MIME and SECURITY. Returned strings have static lifetime.
 */
#ifndef MULIB_MIME_H
#define MULIB_MIME_H
#include "mulib-seurity.h"
#ifdef __cplusplus
extern "C"
{
#endif
  const char *mulib_mime_type (const char *filename);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_MIME) && !defined(MULIB_MIME_IMPLEMENTED)
#define MULIB_MIME_IMPLEMENTED
#include <string.h>
const char *
mulib_mime_type (const char *s)
{
  static const char *const types[][2]
      = { { "html", "text/html; charset=utf-8" },
          { "htm", "text/html; charset=utf-8" },
          { "css", "text/css; charset=utf-8" },
          { "js", "text/javascript; charset=utf-8" },
          { "json", "application/json" },
          { "txt", "text/plain; charset=utf-8" },
          { "svg", "image/svg+xml" },
          { "png", "image/png" },
          { "jpg", "image/jpeg" },
          { "jpeg", "image/jpeg" },
          { "gif", "image/gif" },
          { "ico", "image/x-icon" },
          { "pdf", "application/pdf" },
          { "wasm", "application/wasm" },
          { "xml", "application/xml" } };
  const char *dot = NULL;
  if (s)
    for (; *s; ++s)
      {
        if (*s == '/' || *s == '\\')
          dot = NULL;
        if (*s == '.')
          dot = s + 1;
      }
  if (dot)
    for (size_t i = 0; i < sizeof (types) / sizeof (types[0]); ++i)
      if (mulib_security_ascii_equal (dot, types[i][0]))
        return types[i][1];
  return "application/octet-stream";
}
#endif
