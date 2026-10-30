/** @file mulib-shadowsocks.h
 * @brief SIP004 Shadowsocks AEAD TCP chunk framing with explicit crypto
 * backend. Define MULIB_IMPL_SHADOWSOCKS once. Caller supplies fresh session
 * salt, HKDF-SHA1 subkey derivation and AES-GCM/ChaCha20-Poly1305
 * implementation.
 */
#ifndef MULIB_SHADOWSOCKS_H
#define MULIB_SHADOWSOCKS_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief AEAD callback, 1 success/0 authentication/backend error. Seal
   * writes n+16 bytes; open receives n ciphertext bytes including tag and
   * writes n-16. Nonce is 12 bytes, no AAD. Must be stateless with respect to
   * explicit nonce, permit repeated open calls after incomplete input, and
   * never overrun output. */
  typedef int (*mulib_shadowsocks_aead_fn) (void *,
                                            const unsigned char nonce[12],
                                            const void *, size_t, void *);
  typedef struct mulib_shadowsocks
  {
    void *context;
    mulib_shadowsocks_aead_fn cipher;
    uint64_t nonce;
    int decrypt, failed;
  } mulib_shadowsocks_t;
  /** @brief Initialize a single direction with a freshly derived subkey
   * backend. decrypt=0 seal, 1 open. Never reuse key/salt across streams. 1
   * success. */
  int mulib_shadowsocks_init (mulib_shadowsocks_t *, int decrypt,
                              mulib_shadowsocks_aead_fn, void *);
  /** @brief Seal length and payload with consecutive nonces. Max 16383 bytes.
   * 1 success, -1 invalid/capacity, -2 sticky crypto/nonce failure. used=0 on
   * failure; output may change on seal failure. Nonoverlapping buffers. */
  int mulib_shadowsocks_seal (mulib_shadowsocks_t *, const void *, size_t,
                              void *, size_t, size_t *used);
  /** @brief Open one complete chunk: 1 success, 0 incomplete, -1
   * invalid/capacity, -2 sticky authentication/format/nonce failure. Output
   * unchanged until both tags verify. used/count=0 on failure, nonce advances
   * only on success. */
  int mulib_shadowsocks_open (mulib_shadowsocks_t *, const void *, size_t,
                              void *, size_t, size_t *count, size_t *used);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SHADOWSOCKS) && !defined(MULIB_SHADOWSOCKS_IMPLEMENTED)
#define MULIB_SHADOWSOCKS_IMPLEMENTED
#include <string.h>
static void
mulib_shadowsocks_nonce_ (uint64_t v, unsigned char out[12])
{
  memset (out, 0, 12);
  for (unsigned i = 0; i < 8; ++i)
    out[i] = (unsigned char)(v >> (8 * i));
}
int
mulib_shadowsocks_init (mulib_shadowsocks_t *s, int decrypt,
                        mulib_shadowsocks_aead_fn fn, void *ctx)
{
  if (!s || !fn || (decrypt != 0 && decrypt != 1))
    return 0;
  s->context = ctx;
  s->cipher = fn;
  s->nonce = 0;
  s->decrypt = decrypt;
  s->failed = 0;
  return 1;
}
int
mulib_shadowsocks_seal (mulib_shadowsocks_t *s, const void *p, size_t n,
                        void *output, size_t cap, size_t *used)
{
  unsigned char *out = output, nonce[12], length[2];
  if (used)
    *used = 0;
  if (!s || !s->cipher || s->decrypt || (!p && n) || !out || !used || n > 16383
      || cap < n + 34)
    return -1;
  if (s->failed)
    return -2;
  if (s->nonce > UINT64_MAX - 2)
    {
      s->failed = 1;
      return -2;
    }
  length[0] = (unsigned char)(n >> 8);
  length[1] = (unsigned char)n;
  mulib_shadowsocks_nonce_ (s->nonce, nonce);
  if (s->cipher (s->context, nonce, length, 2, out) != 1)
    {
      s->failed = 1;
      return -2;
    }
  mulib_shadowsocks_nonce_ (s->nonce + 1, nonce);
  if (s->cipher (s->context, nonce, p, n, out + 18) != 1)
    {
      s->failed = 1;
      return -2;
    }
  s->nonce += 2;
  *used = n + 34;
  return 1;
}
int
mulib_shadowsocks_open (mulib_shadowsocks_t *s, const void *data, size_t n,
                        void *out, size_t cap, size_t *count, size_t *used)
{
  const unsigned char *p = data;
  unsigned char nonce[12], length[2], plain[16383];
  size_t len;
  if (used)
    *used = 0;
  if (count)
    *count = 0;
  if (!s || !s->cipher || !s->decrypt || (!p && n) || (!out && cap) || !used
      || !count)
    return -1;
  if (s->failed)
    return -2;
  if (s->nonce > UINT64_MAX - 2)
    {
      s->failed = 1;
      return -2;
    }
  if (n < 18)
    return 0;
  mulib_shadowsocks_nonce_ (s->nonce, nonce);
  if (s->cipher (s->context, nonce, p, 18, length) != 1)
    {
      s->failed = 1;
      return -2;
    }
  len = (size_t)length[0] * 256 + length[1];
  if (len > 16383)
    {
      s->failed = 1;
      return -2;
    }
  if (len > cap)
    return -1;
  if (n < len + 34)
    return 0;
  mulib_shadowsocks_nonce_ (s->nonce + 1, nonce);
  if (s->cipher (s->context, nonce, p + 18, len + 16, plain) != 1)
    {
      s->failed = 1;
      return -2;
    }
  if (len)
    memcpy (out, plain, len);
  memset (plain, 0, sizeof (plain));
  s->nonce += 2;
  *count = len;
  *used = len + 34;
  return 1;
}
#endif
