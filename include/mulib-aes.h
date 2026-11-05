/** @file mulib-aes.h
 * @brief AES-256 block/CTR and encrypt-then-HMAC-SHA256 authenticated messages.
 * Define MULIB_IMPL_AES and AUTHCRYPTO once. Not independently audited.
 */
#ifndef MULIB_AES_H
#define MULIB_AES_H
#include "mulib-authcrypto.h"
#ifdef __cplusplus
extern "C" {
#endif
/** Encrypt one disjoint or in-place 16-byte block with a 32-byte key. */
int mulib_aes256_block(const unsigned char key[32], const unsigned char in[16], unsigned char out[16]);
/** CTR with a unique 12-byte nonce and big-endian counter starting at one.
 * In-place is allowed; other overlap is not. Maximum (2^32-1)*16 bytes. */
int mulib_aes256_ctr(const unsigned char key[32], const unsigned char nonce[12],
                    const void *input, size_t length, void *output);
/** Encrypt with key[0..31], authenticate with independent key[32..63].
 * Nonces MUST never repeat for an encryption key. No entropy is generated.
 * Tag covers nonce, encoded AAD length, AAD and ciphertext. Returns 1/0.
 * AAD/tag/key/nonce must not overlap output. Output unchanged on failure. */
int mulib_aes_seal(const unsigned char key[64], const unsigned char nonce[12],
                   const void *aad, size_t aad_length, const void *plain, size_t length,
                   void *cipher, unsigned char tag[32]);
/** Verify before decrypting. Bad tags leave output unchanged. */
int mulib_aes_open(const unsigned char key[64], const unsigned char nonce[12],
                   const void *aad, size_t aad_length, const void *cipher, size_t length,
                   const unsigned char tag[32], void *plain);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_AES) && !defined(MULIB_AES_IMPLEMENTED)
#define MULIB_AES_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
/* Original scalar FIPS 197 transformations and the standard's S-box constants.
 * Table lookups are not claimed to be resistant to cache side channels. */
static unsigned char mulib_aes_mul_(unsigned char a, unsigned char b) {
  unsigned char r = 0;
  for (unsigned i = 0; i < 8; ++i) {
    r ^= (unsigned char)(a & (unsigned char)-(int)(b & 1));
    a = (unsigned char)((a << 1) ^ (0x1b & -(int)(a >> 7))); b >>= 1;
  }
  return r;
}
static unsigned char mulib_aes_sub_(unsigned char a) {
  static const unsigned char table[256] = {
    0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5,0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76,
    0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0,0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0,
    0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc,0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15,
    0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a,0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75,
    0x09,0x83,0x2c,0x1a,0x1b,0x6e,0x5a,0xa0,0x52,0x3b,0xd6,0xb3,0x29,0xe3,0x2f,0x84,
    0x53,0xd1,0x00,0xed,0x20,0xfc,0xb1,0x5b,0x6a,0xcb,0xbe,0x39,0x4a,0x4c,0x58,0xcf,
    0xd0,0xef,0xaa,0xfb,0x43,0x4d,0x33,0x85,0x45,0xf9,0x02,0x7f,0x50,0x3c,0x9f,0xa8,
    0x51,0xa3,0x40,0x8f,0x92,0x9d,0x38,0xf5,0xbc,0xb6,0xda,0x21,0x10,0xff,0xf3,0xd2,
    0xcd,0x0c,0x13,0xec,0x5f,0x97,0x44,0x17,0xc4,0xa7,0x7e,0x3d,0x64,0x5d,0x19,0x73,
    0x60,0x81,0x4f,0xdc,0x22,0x2a,0x90,0x88,0x46,0xee,0xb8,0x14,0xde,0x5e,0x0b,0xdb,
    0xe0,0x32,0x3a,0x0a,0x49,0x06,0x24,0x5c,0xc2,0xd3,0xac,0x62,0x91,0x95,0xe4,0x79,
    0xe7,0xc8,0x37,0x6d,0x8d,0xd5,0x4e,0xa9,0x6c,0x56,0xf4,0xea,0x65,0x7a,0xae,0x08,
    0xba,0x78,0x25,0x2e,0x1c,0xa6,0xb4,0xc6,0xe8,0xdd,0x74,0x1f,0x4b,0xbd,0x8b,0x8a,
    0x70,0x3e,0xb5,0x66,0x48,0x03,0xf6,0x0e,0x61,0x35,0x57,0xb9,0x86,0xc1,0x1d,0x9e,
    0xe1,0xf8,0x98,0x11,0x69,0xd9,0x8e,0x94,0x9b,0x1e,0x87,0xe9,0xce,0x55,0x28,0xdf,
    0x8c,0xa1,0x89,0x0d,0xbf,0xe6,0x42,0x68,0x41,0x99,0x2d,0x0f,0xb0,0x54,0xbb,0x16
  };
  return table[a];
}
static void mulib_aes_expand_(const unsigned char *key, unsigned char rk[240]) {
  unsigned char t[4], rc = 1;
  memcpy(rk, key, 32);
  for (size_t i = 32; i < 240; i += 4) {
    memcpy(t, rk+i-4, 4);
    if (i % 32 == 0) {
      unsigned char v = t[0]; t[0] = mulib_aes_sub_(t[1]) ^ rc;
      t[1] = mulib_aes_sub_(t[2]); t[2] = mulib_aes_sub_(t[3]); t[3] = mulib_aes_sub_(v);
      rc = mulib_aes_mul_(rc, 2);
    } else if (i % 32 == 16) for (unsigned j = 0; j < 4; ++j) t[j] = mulib_aes_sub_(t[j]);
    for (unsigned j = 0; j < 4; ++j) rk[i+j] = rk[i+j-32] ^ t[j];
  }
  mulib_authcrypto_wipe(t, sizeof t);
}
static void mulib_aes_encrypt_(const unsigned char rk[240], const unsigned char *in, unsigned char *out) {
  unsigned char s[16], t[16];
  for (unsigned i = 0; i < 16; ++i) s[i] = in[i] ^ rk[i];
  for (unsigned round = 1; round <= 14; ++round) {
    for (unsigned c = 0; c < 4; ++c)
      for (unsigned r = 0; r < 4; ++r) t[4*c+r] = mulib_aes_sub_(s[4*((c+r)%4)+r]);
    if (round < 14) for (unsigned c = 0; c < 4; ++c) {
      unsigned char *p = t+4*c, a=p[0], b=p[1], d=p[2], e=p[3];
      s[4*c] = mulib_aes_mul_(a,2)^mulib_aes_mul_(b,3)^d^e;
      s[4*c+1] = a^mulib_aes_mul_(b,2)^mulib_aes_mul_(d,3)^e;
      s[4*c+2] = a^b^mulib_aes_mul_(d,2)^mulib_aes_mul_(e,3);
      s[4*c+3] = mulib_aes_mul_(a,3)^b^d^mulib_aes_mul_(e,2);
    } else memcpy(s, t, 16);
    for (unsigned i = 0; i < 16; ++i) s[i] ^= rk[16*round+i];
  }
  memcpy(out, s, 16); mulib_authcrypto_wipe(s, 16); mulib_authcrypto_wipe(t, 16);
}
int mulib_aes256_block(const unsigned char key[32], const unsigned char in[16], unsigned char out[16]) {
  unsigned char rk[240];
  if (!key || !in || !out) return 0;
  mulib_aes_expand_(key, rk); mulib_aes_encrypt_(rk, in, out);
  mulib_authcrypto_wipe(rk, sizeof rk); return 1;
}
int mulib_aes256_ctr(const unsigned char key[32], const unsigned char nonce[12],
                    const void *input, size_t n, void *output) {
  unsigned char rk[240], counter[16], stream[16];
  const unsigned char *in = (const unsigned char *)input;
  unsigned char *out = (unsigned char *)output;
  uint32_t count = 1;
  if (!key || !nonce || (!in && n) || (!out && n) || (uint64_t)n > UINT64_C(68719476720)) return 0;
  mulib_aes_expand_(key, rk); memcpy(counter, nonce, 12);
  while (n) {
    size_t take = n < 16 ? n : 16;
    for (unsigned i = 0; i < 4; ++i) counter[15-i] = (unsigned char)(count >> (8*i));
    mulib_aes_encrypt_(rk, counter, stream);
    for (size_t i = 0; i < take; ++i) out[i] = in[i] ^ stream[i];
    in += take; out += take; n -= take; ++count;
  }
  mulib_authcrypto_wipe(rk, sizeof rk); mulib_authcrypto_wipe(stream, 16);
  mulib_authcrypto_wipe(counter, 16); return 1;
}
static unsigned char *mulib_aes_macbuf_(const unsigned char *nonce, const void *aad,
                                       size_t an, size_t n) {
  unsigned char *p;
  if ((!aad && an) || an > SIZE_MAX-20 || n > SIZE_MAX-20-an
      || (uint64_t)an > UINT64_MAX/8-84 || (uint64_t)n > UINT64_MAX/8-84-(uint64_t)an) return NULL;
  p = (unsigned char *)malloc(20+an+n);
  if (!p) return NULL;
  memcpy(p, nonce, 12);
  for (unsigned i = 0; i < 8; ++i) p[19-i] = (unsigned char)((uint64_t)an >> (8*i));
  if (an) memcpy(p+20, aad, an);
  return p;
}
int mulib_aes_seal(const unsigned char key[64], const unsigned char nonce[12],
                   const void *aad, size_t an, const void *in, size_t n, void *out, unsigned char tag[32]) {
  unsigned char *p;
  if (!key || !nonce || !tag || (!in && n) || (!out && n)
      || (uint64_t)n > UINT64_C(68719476720)) return 0;
  p = mulib_aes_macbuf_(nonce, aad, an, n); if (!p) return 0;
  (void)mulib_aes256_ctr(key, nonce, in, n, p+20+an);
  (void)mulib_authcrypto_hmac(MULIB_AUTHCRYPTO_SHA256, key+32, 32, p, 20+an+n, tag, 32);
  if (n) memcpy(out, p+20+an, n);
  mulib_authcrypto_wipe(p, 20+an+n); free(p); return 1;
}
int mulib_aes_open(const unsigned char key[64], const unsigned char nonce[12],
                   const void *aad, size_t an, const void *in, size_t n, const unsigned char tag[32], void *out) {
  unsigned char *p, actual[32]; unsigned diff = 0;
  if (!key || !nonce || !tag || (!in && n) || (!out && n)
      || (uint64_t)n > UINT64_C(68719476720)) return 0;
  p = mulib_aes_macbuf_(nonce, aad, an, n); if (!p) return 0;
  if (n) memcpy(p+20+an, in, n);
  (void)mulib_authcrypto_hmac(MULIB_AUTHCRYPTO_SHA256, key+32, 32, p, 20+an+n, actual, 32);
  for (unsigned i = 0; i < 32; ++i) diff |= actual[i] ^ tag[i];
  mulib_authcrypto_wipe(p, 20+an+n); free(p); mulib_authcrypto_wipe(actual, 32);
  return diff ? 0 : mulib_aes256_ctr(key, nonce, in, n, out);
}
#endif
