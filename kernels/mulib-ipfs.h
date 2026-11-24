/** @file mulib-ipfs.h
 * @brief IPFS CIDv1 raw/dag-pb SHA-256 identifiers and verified block
 * callbacks. Define MULIB_IMPL_IPFS and MULIB_IMPL_VARINT once. No
 * daemon/DHT/network or SHA-256 implementation is implicit; block verification
 * uses caller hashing.
 */
#ifndef MULIB_IPFS_H
#define MULIB_IPFS_H
#include "mulib-varint.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_ipfs_cid
  {
    uint64_t codec;
    unsigned char digest[32];
  } mulib_ipfs_cid_t;
  /** @brief Canonical binary CIDv1 (36 bytes), codecs raw=0x55, dag-pb=0x70,
   * multihash sha2-256=0x12, digest length=32. 1 success/0 malformed. */
  int mulib_ipfs_cid_encode (const mulib_ipfs_cid_t *, unsigned char out[36]);
  int mulib_ipfs_cid_decode (const void *, size_t, mulib_ipfs_cid_t *);
  /** @brief Lowercase base32 multibase text (59 characters plus NUL), exact
   * canonical syntax only. No CIDv0/base58. Outputs unchanged on failure. */
  int mulib_ipfs_cid_format (const mulib_ipfs_cid_t *, char out[60]);
  int mulib_ipfs_cid_parse (const char *, size_t, mulib_ipfs_cid_t *);
  /** @brief Hash a block with caller SHA-256 and compare all 32 bytes.
   * Callback returns 1 on success. 1 match, 0 mismatch/backend/argument
   * failure. */
  int mulib_ipfs_verify (const mulib_ipfs_cid_t *, const void *, size_t,
                         int (*sha256) (void *, const void *, size_t,
                                        unsigned char[32]),
                         void *);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_IPFS) && !defined(MULIB_IPFS_IMPLEMENTED)
#define MULIB_IPFS_IMPLEMENTED
#include <string.h>
int
mulib_ipfs_cid_encode (const mulib_ipfs_cid_t *c, unsigned char out[36])
{
  unsigned char v[10];
  size_t n;
  if (!c || !out || (c->codec != 0x55 && c->codec != 0x70))
    return 0;
  out[0] = 1;
  n = mulib_varint_encode (c->codec, v);
  memcpy (out + 1, v, n);
  out[2] = 0x12;
  out[3] = 32;
  memcpy (out + 4, c->digest, 32);
  return 1;
}
int
mulib_ipfs_cid_decode (const void *data, size_t n, mulib_ipfs_cid_t *out)
{
  const unsigned char *p = data;
  mulib_ipfs_cid_t c;
  if (!p || !out || n != 36 || p[0] != 1 || (p[1] != 0x55 && p[1] != 0x70)
      || p[2] != 0x12 || p[3] != 32)
    return 0;
  c.codec = p[1];
  memcpy (c.digest, p + 4, 32);
  *out = c;
  return 1;
}
int
mulib_ipfs_cid_format (const mulib_ipfs_cid_t *c, char out[60])
{
  static const char alphabet[] = "abcdefghijklmnopqrstuvwxyz234567";
  unsigned char bytes[36];
  unsigned acc = 0, bits = 0;
  size_t pos = 1;
  if (!out || !mulib_ipfs_cid_encode (c, bytes))
    return 0;
  out[0] = 'b';
  for (unsigned i = 0; i < 36; ++i)
    {
      acc = (acc << 8) | bytes[i];
      bits += 8;
      while (bits >= 5)
        {
          bits -= 5;
          out[pos++] = alphabet[(acc >> bits) & 31];
        }
    }
  if (bits)
    out[pos++] = alphabet[(acc << (5 - bits)) & 31];
  out[pos] = 0;
  return 1;
}
int
mulib_ipfs_cid_parse (const char *text, size_t n, mulib_ipfs_cid_t *out)
{
  unsigned char bytes[36];
  unsigned acc = 0, bits = 0;
  size_t pos = 0;
  if (!text || !out || n != 59 || text[0] != 'b')
    return 0;
  for (size_t i = 1; i < n; ++i)
    {
      unsigned char c = (unsigned char)text[i];
      unsigned v;
      if (c >= 'a' && c <= 'z')
        v = c - 'a';
      else if (c >= '2' && c <= '7')
        v = c - '2' + 26;
      else
        return 0;
      acc = (acc << 5) | v;
      bits += 5;
      if (bits >= 8)
        {
          bits -= 8;
          if (pos >= 36)
            return 0;
          bytes[pos++] = (unsigned char)(acc >> bits);
        }
    }
  if (pos != 36 || (acc & ((1u << bits) - 1)))
    return 0;
  return mulib_ipfs_cid_decode (bytes, 36, out);
}
int
mulib_ipfs_verify (const mulib_ipfs_cid_t *c, const void *p, size_t n,
                   int (*hash) (void *, const void *, size_t,
                                unsigned char[32]),
                   void *ctx)
{
  unsigned char digest[32];
  unsigned difference = 0;
  if (!c || (c->codec != 0x55 && c->codec != 0x70) || (!p && n) || !hash
      || hash (ctx, p, n, digest) != 1)
    return 0;
  for (unsigned i = 0; i < 32; ++i)
    difference |= digest[i] ^ c->digest[i];
  return difference == 0;
}
#endif
