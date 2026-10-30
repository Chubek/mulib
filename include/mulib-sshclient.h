/** @file mulib-sshclient.h
 * @brief SSH-2 identification and pre-key-exchange binary packet codecs.
 * Define MULIB_IMPL_SSHCLIENT and MULIB_IMPL_ENDIAN once. These codecs do
 * not perform key exchange, encryption, MAC verification or host
 * authentication.
 */
#ifndef MULIB_SSHCLIENT_H
#define MULIB_SSHCLIENT_H
#include "mulib-endian.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Encode SSH-2.0-software CRLF, printable nonspace ASCII software
   * identifier <=244 bytes. 1 success/0 invalid or insufficient capacity. */
  int mulib_sshclient_identification (const char *, void *, size_t,
                                      size_t *used);
  /** @brief Parse one identification line (no banner skipping). 1 success,
   * 0 incomplete, -1 malformed/unsupported. Copies software+comments to a
   * NUL-terminated output. Only SSH-2.0 accepted, maximum 255 wire bytes. */
  int mulib_sshclient_identify (const void *, size_t, char *, size_t,
                                size_t *used);
  /** @brief Encode an unencrypted SSH packet, block alignment 8 bytes, at
   * least 4 padding bytes. rng must fill padding with random bytes and
   * return 1. Payload length 1..32768. 1 success/0 failure; output may change
   * on RNG failure. */
  int mulib_sshclient_packet_encode (const void *, size_t, void *, size_t,
                                     size_t *used,
                                     int (*rng) (void *, void *, size_t),
                                     void *);
  /** @brief Decode unencrypted packet; borrowed payload. 1 complete, 0 need
   * more, -1 invalid. Strict 8-byte alignment and bounded payload. */
  int mulib_sshclient_packet_decode (const void *, size_t,
                                     const unsigned char **, size_t *length,
                                     size_t *used);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SSHCLIENT) && !defined(MULIB_SSHCLIENT_IMPLEMENTED)
#define MULIB_SSHCLIENT_IMPLEMENTED
#include <string.h>
int
mulib_sshclient_identification (const char *name, void *out, size_t cap,
                                size_t *used)
{
  size_t n;
  if (used)
    *used = 0;
  if (!name || !out || !used)
    return 0;
  n = strlen (name);
  if (!n || n > 244 || cap < n + 10)
    return 0;
  for (size_t i = 0; i < n; ++i)
    if ((unsigned char)name[i] < 33 || (unsigned char)name[i] > 126)
      return 0;
  memcpy (out, "SSH-2.0-", 8);
  memcpy ((unsigned char *)out + 8, name, n);
  memcpy ((unsigned char *)out + 8 + n, "\r\n", 2);
  *used = n + 10;
  return 1;
}
int
mulib_sshclient_identify (const void *data, size_t n, char *out, size_t cap,
                          size_t *used)
{
  const unsigned char *p = data;
  size_t i;
  if (used)
    *used = 0;
  if ((!p && n) || !out || !used)
    return -1;
  for (i = 0; i < n && i < 255 && p[i] != '\n'; ++i)
    {
      if (i < 8 && p[i] != (unsigned char)"SSH-2.0-"[i])
        return -1;
      if (p[i] < 32 && p[i] != '\r')
        return -1;
      if (p[i] > 126)
        return -1;
      if (p[i] == '\r' && i + 1 < n && p[i + 1] != '\n')
        return -1;
    }
  if (i == 255)
    return -1;
  if (i == n)
    return 0;
  if (i < 10 || p[i - 1] != '\r' || p[8] == ' ' || cap <= i - 9)
    return -1;
  memcpy (out, p + 8, i - 9);
  out[i - 9] = 0;
  *used = i + 1;
  return 1;
}
int
mulib_sshclient_packet_encode (const void *data, size_t n, void *out,
                               size_t cap, size_t *used,
                               int (*rng) (void *, void *, size_t), void *ctx)
{
  unsigned char *p = out;
  size_t padding, total;
  if (used)
    *used = 0;
  if (!data || !n || n > 32768 || !out || !used || !rng)
    return 0;
  padding = 8 - ((n + 5) % 8);
  if (padding < 4)
    padding += 8;
  total = n + padding + 5;
  if (cap < total)
    return 0;
  if (rng (ctx, p + 5 + n, padding) != 1)
    return 0;
  mulib_endian_store_be32 (p, (uint32_t)(total - 4));
  p[4] = (unsigned char)padding;
  memcpy (p + 5, data, n);
  *used = total;
  return 1;
}
int
mulib_sshclient_packet_decode (const void *data, size_t n,
                               const unsigned char **payload, size_t *length,
                               size_t *used)
{
  const unsigned char *p = data;
  size_t packet, len;
  if (used)
    *used = 0;
  if ((!p && n) || !payload || !length || !used)
    return -1;
  if (n < 4)
    return 0;
  packet = mulib_endian_load_be32 (p);
  if (packet < 6 || packet > 32780 || (packet + 4) % 8)
    return -1;
  if (n < 5)
    return 0;
  if (p[4] < 4 || (size_t)p[4] + 1 >= packet)
    return -1;
  len = packet - p[4] - 1;
  if (len > 32768)
    return -1;
  if (n < packet + 4)
    return 0;
  *payload = p + 5;
  *length = len;
  *used = packet + 4;
  return 1;
}
#endif
