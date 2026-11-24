/** @file mulib-bittorrent.h
 * @brief BitTorrent v1 peer handshake, bounded peer-wire messages and piece
 * SHA-1. Define MULIB_IMPL_BITTORRENT, MULIB_IMPL_ENDIAN and MULIB_IMPL_GIT
 * once.
 */
#ifndef MULIB_BITTORRENT_H
#define MULIB_BITTORRENT_H
#include "mulib-endian.h"
#include "mulib-git.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_bittorrent_handshake
  {
    unsigned char reserved[8], info_hash[20], peer_id[20];
  } mulib_bittorrent_handshake_t;
  typedef struct mulib_bittorrent_message
  {
    unsigned id;
    const unsigned char *data;
    size_t length;
  } mulib_bittorrent_message_t;
  /** @brief Handshake codec, 68 bytes. Decode 1 complete, 0 incomplete, -1
   * invalid; encode 1/0. Reserved extension bits preserved, not interpreted.
   */
  int mulib_bittorrent_handshake_encode (const mulib_bittorrent_handshake_t *,
                                         void *, size_t);
  int mulib_bittorrent_handshake_decode (const void *, size_t,
                                         mulib_bittorrent_handshake_t *);
  /** @brief Peer-wire message ids 0..9, 255 keepalive. Data excludes
   * id/length. Maximum frame 1 MiB, piece blocks <=16 KiB, requests nonzero
   * <=16 KiB. 1 complete, 0 incomplete, -1 malformed, -2 unsupported id.
   * used=0 on error. */
  int mulib_bittorrent_decode (const void *, size_t,
                               mulib_bittorrent_message_t *, size_t *used);
  /** @brief Encode validated message, 1 success/0 invalid/capacity. */
  int mulib_bittorrent_encode (const mulib_bittorrent_message_t *, void *,
                               size_t, size_t *used);
  /** @brief Compare SHA-1 of a complete piece with expected 20-byte digest. */
  int mulib_bittorrent_piece_verify (const void *, size_t,
                                     const unsigned char expected[20]);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_BITTORRENT) && !defined(MULIB_BITTORRENT_IMPLEMENTED)
#define MULIB_BITTORRENT_IMPLEMENTED
#include <string.h>
int
mulib_bittorrent_handshake_encode (const mulib_bittorrent_handshake_t *h,
                                   void *out, size_t n)
{
  unsigned char *p = out;
  if (!h || !p || n < 68)
    return 0;
  p[0] = 19;
  memcpy (p + 1, "BitTorrent protocol", 19);
  memcpy (p + 20, h->reserved, 8);
  memcpy (p + 28, h->info_hash, 20);
  memcpy (p + 48, h->peer_id, 20);
  return 1;
}
int
mulib_bittorrent_handshake_decode (const void *data, size_t n,
                                   mulib_bittorrent_handshake_t *out)
{
  const unsigned char *p = data;
  if ((!p && n) || !out)
    return -1;
  if (n && p[0] != 19)
    return -1;
  if (n >= 20 && memcmp (p + 1, "BitTorrent protocol", 19))
    return -1;
  if (n < 68)
    return 0;
  memcpy (out->reserved, p + 20, 8);
  memcpy (out->info_hash, p + 28, 20);
  memcpy (out->peer_id, p + 48, 20);
  return 1;
}
static int
mulib_bittorrent_valid_ (unsigned id, const unsigned char *p, size_t n)
{
  if ((!p && n) || n > 1048575)
    return 0;
  if (id <= 3 || id == 255)
    return n == 0;
  if (id == 4)
    return n == 4;
  if (id == 5)
    return n > 0;
  if (id == 6 || id == 8)
    {
      uint32_t length, off;
      if (n != 12)
        return 0;
      off = mulib_endian_load_be32 (p + 4);
      length = mulib_endian_load_be32 (p + 8);
      return length && length <= 16384 && off <= UINT32_MAX - length;
    }
  if (id == 7)
    return n > 8 && n <= 16392
           && mulib_endian_load_be32 (p + 4) <= UINT32_MAX - (n - 8);
  if (id == 9)
    return n == 2;
  return -1;
}
int
mulib_bittorrent_decode (const void *data, size_t n,
                         mulib_bittorrent_message_t *out, size_t *used)
{
  const unsigned char *p = data;
  size_t len;
  int r;
  if (used)
    *used = 0;
  if ((!p && n) || !out || !used)
    return -1;
  if (n < 4)
    return 0;
  len = mulib_endian_load_be32 (p);
  if (len > 1048576)
    return -1;
  if (n - 4 < len)
    return 0;
  if (!len)
    {
      out->id = 255;
      out->data = NULL;
      out->length = 0;
      *used = 4;
      return 1;
    }
  if (p[4] == 255)
    return -2;
  r = mulib_bittorrent_valid_ (p[4], p + 5, len - 1);
  if (r != 1)
    return r < 0 ? -2 : -1;
  out->id = p[4];
  out->data = p + 5;
  out->length = len - 1;
  *used = len + 4;
  return 1;
}
int
mulib_bittorrent_encode (const mulib_bittorrent_message_t *v, void *output,
                         size_t cap, size_t *used)
{
  unsigned char *p = output;
  size_t n;
  if (used)
    *used = 0;
  if (!v || !p || !used
      || mulib_bittorrent_valid_ (v->id, v->data, v->length) != 1)
    return 0;
  n = v->id == 255 ? 0 : v->length + 1;
  if (cap < n + 4)
    return 0;
  mulib_endian_store_be32 (p, (uint32_t)n);
  if (n)
    {
      p[4] = (unsigned char)v->id;
      if (v->length)
        memcpy (p + 5, v->data, v->length);
    }
  *used = n + 4;
  return 1;
}
int
mulib_bittorrent_piece_verify (const void *p, size_t n,
                               const unsigned char expected[20])
{
  mulib_git_oid_t id;
  return expected && mulib_git_sha1 (p, n, &id)
         && !memcmp (id.bytes, expected, 20);
}
#endif
