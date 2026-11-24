/** @file mulib-aes-gcm.h
 * @brief NIST AES-128-GCM with 96-bit nonces and 128-bit tags.
 * Define MULIB_IMPL_AES_GCM and ENTROPY. Portable fixed-schedule AES/GHASH;
 * runtime-dispatched AES-NI on supported GCC/Clang x86 hosts. */
#ifndef MULIB_AES_GCM_H
#define MULIB_AES_GCM_H
#include "mulib-entropy.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_aes_gcm {
  unsigned char round_keys[176], hash_key[16];
  unsigned active, hardware;
} mulib_aes_gcm_t;
/** Initialize from a 16-byte key, return 1/0. Invalid arguments leave ctx unchanged.
 * Define MULIB_AES_GCM_NO_INTRINSICS for the portable implementation only. */
int mulib_aes_gcm_init (mulib_aes_gcm_t *, const unsigned char key[16]);
/** Nonce MUST be unique for every encryption with this key. Caller owns nonce
 * allocation. In-place input/output supported, other overlap forbidden; context,
 * nonce, AAD and tag must be disjoint from output. No allocation. Length <=2^36-32.
 * Return 1 success, 0 invalid arguments. */
int mulib_aes_gcm_seal (const mulib_aes_gcm_t *, const unsigned char nonce[12],
    const void *aad, size_t aad_length, const void *plain, size_t length,
    void *cipher, unsigned char tag[16]);
/** Authenticate before releasing plaintext; invalid tags leave output unchanged. */
int mulib_aes_gcm_open (const mulib_aes_gcm_t *, const unsigned char nonce[12],
    const void *aad, size_t aad_length, const void *cipher, size_t length,
    const unsigned char tag[16], void *plain);
void mulib_aes_gcm_clear (mulib_aes_gcm_t *);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_AES_GCM) && !defined(MULIB_AES_GCM_IMPLEMENTED)
#define MULIB_AES_GCM_IMPLEMENTED
#include <string.h>
#if (defined(__i386__) || defined(__x86_64__)) && (defined(__GNUC__) || defined(__clang__)) && !defined(MULIB_AES_GCM_NO_INTRINSICS)
#include <wmmintrin.h>
#define MULIB_AES_GCM_INTRINSICS_ 1
__attribute__((target("aes,sse2"))) static void
mulib_aes_gcm_hardware_ (const unsigned char *keys, const unsigned char *in, unsigned char *out)
{
  __m128i x = _mm_xor_si128 (_mm_loadu_si128 ((const __m128i *)(const void *)in),
      _mm_loadu_si128 ((const __m128i *)(const void *)keys));
  for (unsigned i = 1; i < 10; ++i) x = _mm_aesenc_si128 (x, _mm_loadu_si128 ((const __m128i *)(const void *)(keys + i * 16)));
  x = _mm_aesenclast_si128 (x, _mm_loadu_si128 ((const __m128i *)(const void *)(keys + 160)));
  _mm_storeu_si128 ((__m128i *)(void *)out, x);
}
#endif
static unsigned char mulib_aes_gcm_xtime_ (unsigned char x)
{ return (unsigned char)((x << 1) ^ (0x1b & (0u - (unsigned)(x >> 7)))); }
static unsigned char mulib_aes_gcm_gf_ (unsigned char a, unsigned char b)
{
  unsigned char out = 0;
  for (unsigned i = 0; i < 8; ++i) {
    out ^= (unsigned char)(a & (0u - (unsigned)(b & 1)));
    a = mulib_aes_gcm_xtime_ (a); b >>= 1;
  }
  return out;
}
static unsigned char mulib_aes_gcm_sub_ (unsigned char x)
{
  unsigned char x2 = mulib_aes_gcm_gf_ (x, x), x3 = mulib_aes_gcm_gf_ (x2, x);
  unsigned char x6 = mulib_aes_gcm_gf_ (x3, x3), x12 = mulib_aes_gcm_gf_ (x6, x6);
  unsigned char a = mulib_aes_gcm_gf_ (x12, x3), r;
  for (unsigned i = 0; i < 4; ++i) a = mulib_aes_gcm_gf_ (a, a);
  a = mulib_aes_gcm_gf_ (a, x12); a = mulib_aes_gcm_gf_ (a, x2);
  r = (unsigned char)(a ^ 0x63);
  for (unsigned i = 1; i <= 4; ++i) r ^= (unsigned char)((a << i) | (a >> (8 - i)));
  return r;
}
static void mulib_aes_gcm_block_ (const mulib_aes_gcm_t *c, const unsigned char in[16], unsigned char out[16])
{
  unsigned char s[16], t[16];
#ifdef MULIB_AES_GCM_INTRINSICS_
  if (c->hardware) { mulib_aes_gcm_hardware_ (c->round_keys, in, out); return; }
#endif
  for (unsigned i = 0; i < 16; ++i) s[i] = in[i] ^ c->round_keys[i];
  for (unsigned round = 1; round <= 10; ++round) {
    for (unsigned col = 0; col < 4; ++col)
      for (unsigned row = 0; row < 4; ++row) t[4 * col + row] = mulib_aes_gcm_sub_ (s[4 * ((col + row) % 4) + row]);
    if (round < 10) for (unsigned col = 0; col < 4; ++col) {
      unsigned char *p = t + 4 * col, all = p[0] ^ p[1] ^ p[2] ^ p[3];
      for (unsigned row = 0; row < 4; ++row)
        s[4 * col + row] = p[row] ^ all ^ mulib_aes_gcm_xtime_ ((unsigned char)(p[row] ^ p[(row + 1) % 4]));
    } else memcpy (s, t, 16);
    for (unsigned i = 0; i < 16; ++i) s[i] ^= c->round_keys[16 * round + i];
  }
  memcpy (out, s, 16); mulib_secret_wipe (s, sizeof s); mulib_secret_wipe (t, sizeof t);
}
int mulib_aes_gcm_init (mulib_aes_gcm_t *ctx, const unsigned char key[16])
{
  mulib_aes_gcm_t c;
  unsigned char temp[4], rc = 1, zero[16] = {0};
  if (!ctx || !key) return 0;
  memset (&c, 0, sizeof c); memcpy (c.round_keys, key, 16);
  for (unsigned i = 16; i < 176; i += 4) {
    memcpy (temp, c.round_keys + i - 4, 4);
    if (i % 16 == 0) {
      unsigned char a = temp[0];
      temp[0] = mulib_aes_gcm_sub_ (temp[1]) ^ rc;
      temp[1] = mulib_aes_gcm_sub_ (temp[2]); temp[2] = mulib_aes_gcm_sub_ (temp[3]); temp[3] = mulib_aes_gcm_sub_ (a);
      rc = mulib_aes_gcm_xtime_ (rc);
    }
    for (unsigned j = 0; j < 4; ++j) c.round_keys[i + j] = c.round_keys[i + j - 16] ^ temp[j];
  }
#ifdef MULIB_AES_GCM_INTRINSICS_
  c.hardware = __builtin_cpu_supports ("aes") != 0;
#endif
  c.active = 1; mulib_aes_gcm_block_ (&c, zero, c.hash_key);
  *ctx = c; mulib_secret_wipe (&c, sizeof c); mulib_secret_wipe (temp, sizeof temp); return 1;
}
static uint64_t mulib_aes_gcm_load_ (const unsigned char *p)
{
  uint64_t v = 0;
  for (unsigned i = 0; i < 8; ++i) v = (v << 8) | p[i];
  return v;
}
static void mulib_aes_gcm_store_ (unsigned char *p, uint64_t v)
{ for (unsigned i = 0; i < 8; ++i) p[7 - i] = (unsigned char)(v >> (8 * i)); }
static void mulib_aes_gcm_ghash_ (unsigned char s[16], const unsigned char h[16], const unsigned char b[16])
{
  uint64_t xh = mulib_aes_gcm_load_ (s) ^ mulib_aes_gcm_load_ (b);
  uint64_t xl = mulib_aes_gcm_load_ (s + 8) ^ mulib_aes_gcm_load_ (b + 8);
  uint64_t vh = mulib_aes_gcm_load_ (h), vl = mulib_aes_gcm_load_ (h + 8), zh = 0, zl = 0;
  for (unsigned i = 0; i < 128; ++i) {
    uint64_t mask = UINT64_C(0) - (xh >> 63), low = vl & 1;
    zh ^= vh & mask; zl ^= vl & mask;
    xh = (xh << 1) | (xl >> 63); xl <<= 1;
    vl = (vl >> 1) | (vh << 63); vh = (vh >> 1) ^ (UINT64_C(0xe100000000000000) & (UINT64_C(0) - low));
  }
  mulib_aes_gcm_store_ (s, zh); mulib_aes_gcm_store_ (s + 8, zl);
}
static void mulib_aes_gcm_ghash_bytes_ (unsigned char s[16], const unsigned char h[16], const void *data, size_t n)
{
  const unsigned char *p = (const unsigned char *)data;
  unsigned char last[16] = {0};
  while (n >= 16) { mulib_aes_gcm_ghash_ (s, h, p); p += 16; n -= 16; }
  if (n) { memcpy (last, p, n); mulib_aes_gcm_ghash_ (s, h, last); }
  mulib_secret_wipe (last, sizeof last);
}
static void mulib_aes_gcm_tag_ (const mulib_aes_gcm_t *c, const unsigned char nonce[12],
    const void *aad, size_t an, const void *cipher, size_t n, unsigned char tag[16])
{
  unsigned char s[16] = {0}, b[16] = {0}, mask[16];
  mulib_aes_gcm_ghash_bytes_ (s, c->hash_key, aad, an);
  mulib_aes_gcm_ghash_bytes_ (s, c->hash_key, cipher, n);
  mulib_aes_gcm_store_ (b, (uint64_t)an * 8); mulib_aes_gcm_store_ (b + 8, (uint64_t)n * 8);
  mulib_aes_gcm_ghash_ (s, c->hash_key, b);
  memcpy (b, nonce, 12); b[12] = b[13] = b[14] = 0; b[15] = 1;
  mulib_aes_gcm_block_ (c, b, mask);
  for (unsigned i = 0; i < 16; ++i) tag[i] = s[i] ^ mask[i];
  mulib_secret_wipe (s, sizeof s); mulib_secret_wipe (b, sizeof b); mulib_secret_wipe (mask, sizeof mask);
}
static void mulib_aes_gcm_ctr_ (const mulib_aes_gcm_t *c, const unsigned char nonce[12], const void *input, size_t n, void *output)
{
  const unsigned char *in = (const unsigned char *)input;
  unsigned char *out = (unsigned char *)output, counter[16], stream[16];
  uint32_t value = 2;
  memcpy (counter, nonce, 12);
  while (n) {
    size_t take = n > 16 ? 16 : n;
    for (unsigned i = 0; i < 4; ++i) counter[15 - i] = (unsigned char)(value >> (8 * i));
    mulib_aes_gcm_block_ (c, counter, stream);
    for (size_t i = 0; i < take; ++i) out[i] = in[i] ^ stream[i];
    in += take; out += take; n -= take; ++value;
  }
  mulib_secret_wipe (counter, sizeof counter); mulib_secret_wipe (stream, sizeof stream);
}
static int mulib_aes_gcm_args_ (const mulib_aes_gcm_t *c, const void *nonce,
    const void *aad, size_t an, const void *in, size_t n, const void *out, const void *tag)
{
  return c && c->active == 1 && nonce && tag && (aad || !an) && (in || !n) && (out || !n)
      && an <= UINT64_MAX / 8 && (uint64_t)n <= UINT64_C(68719476704);
}
int mulib_aes_gcm_seal (const mulib_aes_gcm_t *c, const unsigned char nonce[12], const void *aad,
    size_t an, const void *in, size_t n, void *out, unsigned char tag[16])
{
  if (!mulib_aes_gcm_args_ (c, nonce, aad, an, in, n, out, tag)) return 0;
  mulib_aes_gcm_ctr_ (c, nonce, in, n, out); mulib_aes_gcm_tag_ (c, nonce, aad, an, out, n, tag); return 1;
}
int mulib_aes_gcm_open (const mulib_aes_gcm_t *c, const unsigned char nonce[12], const void *aad,
    size_t an, const void *in, size_t n, const unsigned char tag[16], void *out)
{
  unsigned char actual[16]; int ok;
  if (!mulib_aes_gcm_args_ (c, nonce, aad, an, in, n, out, tag)) return 0;
  mulib_aes_gcm_tag_ (c, nonce, aad, an, in, n, actual);
  ok = mulib_secret_equal (actual, tag, 16); mulib_secret_wipe (actual, sizeof actual);
  if (ok) mulib_aes_gcm_ctr_ (c, nonce, in, n, out);
  return ok;
}
void mulib_aes_gcm_clear (mulib_aes_gcm_t *c) { if (c) mulib_secret_wipe (c, sizeof *c); }
#undef MULIB_AES_GCM_INTRINSICS_
#endif
