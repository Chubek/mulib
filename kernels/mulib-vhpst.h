/** @file mulib-vhpst.h
 * @brief Virtual-host selection over a borrowed exact DNS-name table.
 * The historical filename is interpreted as vhost. Define MULIB_IMPL_VHPST
 * and SECURITY. No wildcard, port, IP-literal or IDNA normalization. */
#ifndef MULIB_VHPST_H
#define MULIB_VHPST_H
#include "mulib-seurity.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_vhpst
  {
    const char *name;
    void *context;
  } mulib_vhpst_t;
  /** Exact ASCII case-insensitive DNS host; SIZE_MAX for absent/invalid. */
  size_t mulib_vhpst_find (const mulib_vhpst_t *hosts, size_t count,
                           const char *host);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_VHPST) && !defined(MULIB_VHPST_IMPLEMENTED)
#define MULIB_VHPST_IMPLEMENTED
#include <string.h>
size_t
mulib_vhpst_find (const mulib_vhpst_t *h, size_t n, const char *s)
{
  size_t len, label = 0;
  if ((!h && n) || !s || !(len = strlen (s)) || len > 253)
    return SIZE_MAX;
  for (size_t i = 0; i <= len; ++i)
    {
      unsigned c = (unsigned char)s[i];
      if (c == '.' || !c)
        {
          if (!label || label > 63 || s[i - 1] == '-')
            return SIZE_MAX;
          label = 0;
        }
      else
        {
          if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
                || (c >= '0' && c <= '9') || c == '-')
              || (!label && c == '-'))
            return SIZE_MAX;
          ++label;
        }
    }
  for (size_t i = 0; i < n; ++i)
    if (mulib_security_ascii_equal (h[i].name, s))
      return i;
  return SIZE_MAX;
}
#endif
