/** @file mulib-p2pproto.h
 * @brief libp2p multistream-select length-delimited protocol negotiation
 * records. Define MULIB_IMPL_P2PPROTO and MULIB_IMPL_VARINT once.
 * Transport-independent.
 */
#ifndef MULIB_P2PPROTO_H
#define MULIB_P2PPROTO_H
#include "mulib-varint.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Encode newline-terminated protocol token, unsigned-varint length.
   * Tokens start '/' or equal "na"/"ls", printable ASCII, 1..1023 bytes.
   * 1 success/0 invalid/capacity. Output nonoverlapping, used=0 on failure. */
  int mulib_p2pproto_encode (const char *token, void *, size_t, size_t *used);
  /** @brief Decode one canonical frame; copy token without LF to
   * NUL-terminated output. 1 complete, 0 incomplete, -1 malformed/capacity.
   * used=0 on failure. */
  int mulib_p2pproto_decode (const void *, size_t, char *, size_t,
                             size_t *used);
  /** @brief Choose an exact supported protocol token; returns its index or
   * SIZE_MAX. Negotiation greeting is /multistream/1.0.0; unsupported
   * proposals should be answered with an encoded "na". No
   * transport/authentication. */
  size_t mulib_p2pproto_select (const char *proposal,
                                const char *const *supported, size_t count);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_P2PPROTO) && !defined(MULIB_P2PPROTO_IMPLEMENTED)
#define MULIB_P2PPROTO_IMPLEMENTED
#include <string.h>
static int
mulib_p2pproto_valid_ (const char *p, size_t n)
{
  if (!n || n > 1023
      || (p[0] != '/'
          && !(n == 2 && (!memcmp (p, "na", 2) || !memcmp (p, "ls", 2)))))
    return 0;
  for (size_t i = 0; i < n; ++i)
    if ((unsigned char)p[i] < 33 || (unsigned char)p[i] > 126)
      return 0;
  return 1;
}
int
mulib_p2pproto_encode (const char *token, void *output, size_t cap,
                       size_t *used)
{
  unsigned char *out = output, v[10];
  size_t n, k;
  if (used)
    *used = 0;
  if (!token || !out || !used)
    return 0;
  n = strlen (token);
  if (!mulib_p2pproto_valid_ (token, n))
    return 0;
  k = mulib_varint_encode (n + 1, v);
  if (cap < n + k + 1)
    return 0;
  memcpy (out, v, k);
  memcpy (out + k, token, n);
  out[k + n] = '\n';
  *used = n + k + 1;
  return 1;
}
int
mulib_p2pproto_decode (const void *data, size_t n, char *out, size_t cap,
                       size_t *used)
{
  const unsigned char *p = data;
  uint64_t length;
  size_t k;
  unsigned char v[10];
  if (used)
    *used = 0;
  if ((!p && n) || !out || !used)
    return -1;
  if (!n)
    return 0;
  if (!mulib_varint_decode (p, n, &length, &k))
    return n < 2 && (p[0] & 128) ? 0 : -1;
  if (length < 2 || length > 1024 || mulib_varint_encode (length, v) != k)
    return -1;
  if (n - k < length)
    return 0;
  if (p[k + (size_t)length - 1] != '\n' || cap < length
      || !mulib_p2pproto_valid_ ((const char *)p + k, (size_t)length - 1))
    return -1;
  memcpy (out, p + k, (size_t)length - 1);
  out[length - 1] = 0;
  *used = k + (size_t)length;
  return 1;
}
size_t
mulib_p2pproto_select (const char *p, const char *const *list, size_t n)
{
  if (!p || (!list && n) || !mulib_p2pproto_valid_ (p, strlen (p))
      || p[0] != '/')
    return SIZE_MAX;
  for (size_t i = 0; i < n; ++i)
    if (list[i] && !strcmp (p, list[i]))
      return i;
  return SIZE_MAX;
}
#endif
