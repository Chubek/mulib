/** @file mulib-authcrypto.h
 * @brief SHA-1/SHA-256, HMAC and PBKDF2-SHA-256 authentication primitives.
 * Define MULIB_IMPL_AUTHCRYPTO once. No entropy source; SHA-1 is for legacy OTP
 * interoperability only, not signatures. This implementation is not audited. */
#ifndef MULIB_AUTHCRYPTO_H
#define MULIB_AUTHCRYPTO_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef enum mulib_authcrypto_algorithm {
  MULIB_AUTHCRYPTO_SHA1 = 1, MULIB_AUTHCRYPTO_SHA256 = 2
} mulib_authcrypto_algorithm;
/** Digest size, or zero for an unsupported algorithm. */
size_t mulib_authcrypto_size (mulib_authcrypto_algorithm algorithm);
/** Hash bytes. NULL input is allowed only at zero length. Output unchanged on
 * invalid arguments. Output may overlap input. */
int mulib_authcrypto_hash (mulib_authcrypto_algorithm algorithm,
                          const void *data, size_t length,
                          unsigned char *out, size_t capacity);
/** RFC 2104 HMAC, with the same output contract as hash. */
int mulib_authcrypto_hmac (mulib_authcrypto_algorithm algorithm,
                          const void *key, size_t key_length,
                          const void *data, size_t length,
                          unsigned char *out, size_t capacity);
/** Derive exactly 32 bytes. Iterations must be nonzero; the caller must bound
 * cost and choose a password-storage work factor. Not a password record API. */
int mulib_authcrypto_pbkdf2 (const void *password, size_t password_length,
                            const void *salt, size_t salt_length,
                            uint32_t iterations, unsigned char out[32]);
/** Best-effort volatile wipe; cannot erase copies made by compiler/hardware. */
void mulib_authcrypto_wipe (void *data, size_t length);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_AUTHCRYPTO) && !defined(MULIB_AUTHCRYPTO_IMPLEMENTED)
#define MULIB_AUTHCRYPTO_IMPLEMENTED
#include <string.h>
typedef struct mulib_authcrypto_ctx_ {
  uint32_t h[8];
  uint64_t length;
  unsigned char block[64];
  size_t used;
  mulib_authcrypto_algorithm algorithm;
} mulib_authcrypto_ctx_;
void
mulib_authcrypto_wipe (void *p, size_t n)
{
  volatile unsigned char *v = (volatile unsigned char *)p;
  if (v) while (n--) *v++ = 0;
}
size_t
mulib_authcrypto_size (mulib_authcrypto_algorithm a)
{
  return a == MULIB_AUTHCRYPTO_SHA1 ? 20 : a == MULIB_AUTHCRYPTO_SHA256 ? 32 : 0;
}
static uint32_t
mulib_authcrypto_rotr_ (uint32_t x, unsigned n)
{
  return (x >> n) | (x << (32 - n));
}
static void
mulib_authcrypto_block_ (mulib_authcrypto_ctx_ *c)
{
  static const uint32_t k[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
  };
  uint32_t w[80], a=c->h[0], b=c->h[1], d=c->h[3], e=c->h[4],
           f=c->h[5], g=c->h[6], h=c->h[7], v=c->h[2];
  for (unsigned i=0; i<16; ++i)
    w[i] = (uint32_t)c->block[i*4]<<24 | (uint32_t)c->block[i*4+1]<<16
           | (uint32_t)c->block[i*4+2]<<8 | c->block[i*4+3];
  if (c->algorithm == MULIB_AUTHCRYPTO_SHA1)
    {
      for (unsigned i=16; i<80; ++i)
        w[i] = mulib_authcrypto_rotr_ (w[i-3]^w[i-8]^w[i-14]^w[i-16], 31);
      for (unsigned i=0; i<80; ++i)
        {
          uint32_t choice = i<20 ? (b&v)|(~b&d) : i<40 ? b^v^d
                            : i<60 ? (b&v)|(b&d)|(v&d) : b^v^d;
          uint32_t constant = i<20 ? 0x5a827999u : i<40 ? 0x6ed9eba1u
                             : i<60 ? 0x8f1bbcdcu : 0xca62c1d6u;
          uint32_t t = mulib_authcrypto_rotr_ (a,27)+choice+e+constant+w[i];
          e=d; d=v; v=mulib_authcrypto_rotr_ (b,2); b=a; a=t;
        }
    }
  else
    {
      for (unsigned i=16; i<64; ++i)
        {
          uint32_t x=w[i-15], y=w[i-2];
          w[i] = w[i-16]+(mulib_authcrypto_rotr_(x,7)^mulib_authcrypto_rotr_(x,18)^(x>>3))
                 +w[i-7]+(mulib_authcrypto_rotr_(y,17)^mulib_authcrypto_rotr_(y,19)^(y>>10));
        }
      for (unsigned i=0; i<64; ++i)
        {
          uint32_t t = h+(mulib_authcrypto_rotr_(e,6)^mulib_authcrypto_rotr_(e,11)^mulib_authcrypto_rotr_(e,25))
                       +((e&f)^(~e&g))+k[i]+w[i];
          uint32_t u = (mulib_authcrypto_rotr_(a,2)^mulib_authcrypto_rotr_(a,13)^mulib_authcrypto_rotr_(a,22))
                       +((a&b)^(a&v)^(b&v));
          h=g; g=f; f=e; e=d+t; d=v; v=b; b=a; a=t+u;
        }
      c->h[5]+=f; c->h[6]+=g; c->h[7]+=h;
    }
  c->h[0]+=a; c->h[1]+=b; c->h[2]+=v; c->h[3]+=d; c->h[4]+=e;
  mulib_authcrypto_wipe (w, sizeof w);
}
static void
mulib_authcrypto_init_ (mulib_authcrypto_ctx_ *c, mulib_authcrypto_algorithm a)
{
  static const uint32_t s256[8] = {0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
                                  0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
  memset (c, 0, sizeof *c);
  c->algorithm = a;
  if (a == MULIB_AUTHCRYPTO_SHA256) memcpy (c->h, s256, sizeof s256);
  else {
    c->h[0]=0x67452301; c->h[1]=0xefcdab89; c->h[2]=0x98badcfe;
    c->h[3]=0x10325476; c->h[4]=0xc3d2e1f0;
  }
}
static void
mulib_authcrypto_update_ (mulib_authcrypto_ctx_ *c, const void *data, size_t n)
{
  const unsigned char *p = (const unsigned char *)data;
  c->length += (uint64_t)n;
  while (n) {
    size_t take = 64-c->used;
    if (take > n) take=n;
    memcpy (c->block+c->used, p, take);
    p+=take; n-=take; c->used+=take;
    if (c->used == 64) { mulib_authcrypto_block_ (c); c->used=0; }
  }
}
static void
mulib_authcrypto_final_ (mulib_authcrypto_ctx_ *c, unsigned char *out)
{
  uint64_t bits = c->length*8;
  c->block[c->used++]=0x80;
  if (c->used > 56) {
    memset (c->block+c->used, 0, 64-c->used);
    mulib_authcrypto_block_ (c); c->used=0;
  }
  memset (c->block+c->used, 0, 56-c->used);
  for (unsigned i=0; i<8; ++i) c->block[63-i]=(unsigned char)(bits>>(i*8));
  mulib_authcrypto_block_ (c);
  for (size_t i=0; i<mulib_authcrypto_size(c->algorithm); ++i)
    out[i]=(unsigned char)(c->h[i/4]>>(24-8*(i%4)));
  mulib_authcrypto_wipe (c, sizeof *c);
}
int
mulib_authcrypto_hash (mulib_authcrypto_algorithm a, const void *p, size_t n,
                       unsigned char *out, size_t cap)
{
  mulib_authcrypto_ctx_ c;
  size_t size=mulib_authcrypto_size(a);
  if (!size || (!p && n) || !out || cap<size || n>UINT64_MAX/8) return 0;
  mulib_authcrypto_init_ (&c,a);
  mulib_authcrypto_update_ (&c,p,n);
  mulib_authcrypto_final_ (&c,out);
  return 1;
}
static void
mulib_authcrypto_hmac_init_ (mulib_authcrypto_ctx_ *inner,
                             mulib_authcrypto_ctx_ *outer,
                             mulib_authcrypto_algorithm a,
                             const void *key, size_t n)
{
  unsigned char pad[64] = {0};
  if (n>64) (void)mulib_authcrypto_hash(a,key,n,pad,sizeof pad);
  else if (n) memcpy (pad,key,n);
  for (unsigned i=0; i<64; ++i) pad[i]^=0x36;
  mulib_authcrypto_init_ (inner,a); mulib_authcrypto_update_(inner,pad,64);
  for (unsigned i=0; i<64; ++i) pad[i]^=0x36^0x5c;
  mulib_authcrypto_init_ (outer,a); mulib_authcrypto_update_(outer,pad,64);
  mulib_authcrypto_wipe (pad,sizeof pad);
}
static void
mulib_authcrypto_hmac_final_ (mulib_authcrypto_ctx_ *inner,
                              mulib_authcrypto_ctx_ *outer, unsigned char *out)
{
  unsigned char digest[32];
  size_t n=mulib_authcrypto_size(inner->algorithm);
  mulib_authcrypto_final_ (inner,digest);
  mulib_authcrypto_update_ (outer,digest,n);
  mulib_authcrypto_final_ (outer,out);
  mulib_authcrypto_wipe (digest,sizeof digest);
}
int
mulib_authcrypto_hmac (mulib_authcrypto_algorithm a, const void *key, size_t kn,
                       const void *p, size_t n, unsigned char *out, size_t cap)
{
  mulib_authcrypto_ctx_ inner, outer;
  size_t size=mulib_authcrypto_size(a);
  if (!size || (!key && kn) || (!p && n) || !out || cap<size
      || kn>UINT64_MAX/8 || n>UINT64_MAX/8-64) return 0;
  mulib_authcrypto_hmac_init_ (&inner,&outer,a,key,kn);
  mulib_authcrypto_update_ (&inner,p,n);
  mulib_authcrypto_hmac_final_ (&inner,&outer,out);
  return 1;
}
int
mulib_authcrypto_pbkdf2 (const void *password, size_t pn, const void *salt,
                         size_t sn, uint32_t iterations, unsigned char out[32])
{
  mulib_authcrypto_ctx_ base_inner, base_outer, inner, outer;
  unsigned char u[32], result[32], block[4]={0,0,0,1};
  if ((!password && pn) || (!salt && sn) || !out || !iterations
      || pn>UINT64_MAX/8 || sn>UINT64_MAX/8-68) return 0;
  mulib_authcrypto_hmac_init_ (&base_inner,&base_outer,MULIB_AUTHCRYPTO_SHA256,password,pn);
  inner=base_inner; outer=base_outer;
  mulib_authcrypto_update_ (&inner,salt,sn);
  mulib_authcrypto_update_ (&inner,block,4);
  mulib_authcrypto_hmac_final_ (&inner,&outer,u);
  memcpy (result,u,32);
  for (uint32_t i=1; i<iterations; ++i) {
    inner=base_inner; outer=base_outer;
    mulib_authcrypto_update_ (&inner,u,32);
    mulib_authcrypto_hmac_final_ (&inner,&outer,u);
    for (unsigned j=0; j<32; ++j) result[j]^=u[j];
  }
  memcpy (out,result,32);
  mulib_authcrypto_wipe (u,sizeof u); mulib_authcrypto_wipe (result,sizeof result);
  mulib_authcrypto_wipe (&base_inner,sizeof base_inner);
  mulib_authcrypto_wipe (&base_outer,sizeof base_outer);
  return 1;
}
#endif
