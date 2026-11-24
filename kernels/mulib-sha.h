/** @file mulib-sha.h
 * @brief SHA-0/1/2/3 and SHAKE with streaming contexts and SIMD alternatives.
 * SHA-0 (legacy), SHA-1, SHA-2, SHA-3 and SHAKE. Byte-oriented FIPS 180-4/202.
 * Define MULIB_IMPL_SHA in one translation unit. No allocation or dependencies.
 * SHA-0 and SHA-1 are provided for compatibility, not collision resistance.
 * All status functions return 1 on success, 0 on invalid input/state; NULL data
 * is accepted only with zero length. Output must not overlap context or input.
 * final consumes the context; SHAKE squeeze may be repeated. Lengths are bytes.
 * _intrinsics selects SSE2 when compiled in, otherwise the portable path.
 * Define MULIB_HASH_NO_INTRINSICS to compile only portable code.
 */
#ifndef MULIB_SHA_H
#define MULIB_SHA_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef enum {
  MULIB_SHA0,
  MULIB_SHA1,
  MULIB_SHA224,
  MULIB_SHA256,
  MULIB_SHA384,
  MULIB_SHA512,
  MULIB_SHA512_224,
  MULIB_SHA512_256,
  MULIB_SHA3_224,
  MULIB_SHA3_256,
  MULIB_SHA3_384,
  MULIB_SHA3_512,
  MULIB_SHAKE128, MULIB_SHAKE256
} mulib_sha_algorithm;
typedef struct {
  uint64_t state[25], bytes_lo, bytes_hi;
  unsigned char block[128];
  size_t used, rate, digest_size;
  mulib_sha_algorithm algorithm;
  unsigned phase, intrinsics, domain;
} mulib_sha_ctx;
size_t mulib_sha_digest_size(mulib_sha_algorithm algorithm);
int mulib_sha_init(mulib_sha_ctx *ctx, mulib_sha_algorithm algorithm);
int mulib_sha_init_intrinsics(mulib_sha_ctx *ctx, mulib_sha_algorithm algorithm);
int mulib_sha_update(mulib_sha_ctx *ctx, const void *data, size_t length);
int mulib_sha_final(mulib_sha_ctx *ctx, void *out, size_t capacity);
int mulib_sha_squeeze(mulib_sha_ctx *ctx, void *out, size_t length);
int mulib_sha(mulib_sha_algorithm algorithm, const void *data, size_t length,
              void *out, size_t capacity);
int mulib_sha_intrinsics(mulib_sha_algorithm algorithm, const void *data,
                         size_t length, void *out, size_t capacity);
int mulib_sha0(const void *data, size_t length, unsigned char out[20]);
int mulib_sha1(const void *data, size_t length, unsigned char out[20]);
int mulib_sha224(const void *data, size_t length, unsigned char out[28]);
int mulib_sha256(const void *data, size_t length, unsigned char out[32]);
int mulib_sha384(const void *data, size_t length, unsigned char out[48]);
int mulib_sha512(const void *data, size_t length, unsigned char out[64]);
int mulib_sha512_224(const void *data, size_t length, unsigned char out[28]);
int mulib_sha512_256(const void *data, size_t length, unsigned char out[32]);
int mulib_sha3_224(const void *data, size_t length, unsigned char out[28]);
int mulib_sha3_256(const void *data, size_t length, unsigned char out[32]);
int mulib_sha3_384(const void *data, size_t length, unsigned char out[48]);
int mulib_sha3_512(const void *data, size_t length, unsigned char out[64]);
int mulib_shake128(const void *data, size_t length, void *out, size_t out_length);
int mulib_shake256(const void *data, size_t length, void *out, size_t out_length);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SHA) && !defined(MULIB_SHA_IMPLEMENTED)
#define MULIB_SHA_IMPLEMENTED
#include "detail/mulib-hash-core.h"
#ifdef __cplusplus
extern "C" {
#endif
static const uint32_t mulib_sha_k32_[64]={
0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
static const uint64_t mulib_sha_k64_[80]={
UINT64_C(0x428a2f98d728ae22),
UINT64_C(0x7137449123ef65cd),
UINT64_C(0xb5c0fbcfec4d3b2f),
UINT64_C(0xe9b5dba58189dbbc),
UINT64_C(0x3956c25bf348b538),
UINT64_C(0x59f111f1b605d019),
UINT64_C(0x923f82a4af194f9b),
UINT64_C(0xab1c5ed5da6d8118),
UINT64_C(0xd807aa98a3030242),
UINT64_C(0x12835b0145706fbe),
UINT64_C(0x243185be4ee4b28c),
UINT64_C(0x550c7dc3d5ffb4e2),
UINT64_C(0x72be5d74f27b896f),
UINT64_C(0x80deb1fe3b1696b1),
UINT64_C(0x9bdc06a725c71235),
UINT64_C(0xc19bf174cf692694),
UINT64_C(0xe49b69c19ef14ad2),
UINT64_C(0xefbe4786384f25e3),
UINT64_C(0x0fc19dc68b8cd5b5),
UINT64_C(0x240ca1cc77ac9c65),
UINT64_C(0x2de92c6f592b0275),
UINT64_C(0x4a7484aa6ea6e483),
UINT64_C(0x5cb0a9dcbd41fbd4),
UINT64_C(0x76f988da831153b5),
UINT64_C(0x983e5152ee66dfab),
UINT64_C(0xa831c66d2db43210),
UINT64_C(0xb00327c898fb213f),
UINT64_C(0xbf597fc7beef0ee4),
UINT64_C(0xc6e00bf33da88fc2),
UINT64_C(0xd5a79147930aa725),
UINT64_C(0x06ca6351e003826f),
UINT64_C(0x142929670a0e6e70),
UINT64_C(0x27b70a8546d22ffc),
UINT64_C(0x2e1b21385c26c926),
UINT64_C(0x4d2c6dfc5ac42aed),
UINT64_C(0x53380d139d95b3df),
UINT64_C(0x650a73548baf63de),
UINT64_C(0x766a0abb3c77b2a8),
UINT64_C(0x81c2c92e47edaee6),
UINT64_C(0x92722c851482353b),
UINT64_C(0xa2bfe8a14cf10364),
UINT64_C(0xa81a664bbc423001),
UINT64_C(0xc24b8b70d0f89791),
UINT64_C(0xc76c51a30654be30),
UINT64_C(0xd192e819d6ef5218),
UINT64_C(0xd69906245565a910),
UINT64_C(0xf40e35855771202a),
UINT64_C(0x106aa07032bbd1b8),
UINT64_C(0x19a4c116b8d2d0c8),
UINT64_C(0x1e376c085141ab53),
UINT64_C(0x2748774cdf8eeb99),
UINT64_C(0x34b0bcb5e19b48a8),
UINT64_C(0x391c0cb3c5c95a63),
UINT64_C(0x4ed8aa4ae3418acb),
UINT64_C(0x5b9cca4f7763e373),
UINT64_C(0x682e6ff3d6b2b8a3),
UINT64_C(0x748f82ee5defb2fc),
UINT64_C(0x78a5636f43172f60),
UINT64_C(0x84c87814a1f0ab72),
UINT64_C(0x8cc702081a6439ec),
UINT64_C(0x90befffa23631e28),
UINT64_C(0xa4506cebde82bde9),
UINT64_C(0xbef9a3f7b2c67915),
UINT64_C(0xc67178f2e372532b),
UINT64_C(0xca273eceea26619c),
UINT64_C(0xd186b8c721c0c207),
UINT64_C(0xeada7dd6cde0eb1e),
UINT64_C(0xf57d4f7fee6ed178),
UINT64_C(0x06f067aa72176fba),
UINT64_C(0x0a637dc5a2c898a6),
UINT64_C(0x113f9804bef90dae),
UINT64_C(0x1b710b35131c471b),
UINT64_C(0x28db77f523047d84),
UINT64_C(0x32caab7b40c72493),
UINT64_C(0x3c9ebe0a15c9bebc),
UINT64_C(0x431d67c49c100d4c),
UINT64_C(0x4cc5d4becb3e42b6),
UINT64_C(0x597f299cfc657e2a),
UINT64_C(0x5fcb6fab3ad6faec),
UINT64_C(0x6c44198c4a475817)};
static void mulib_sha_compress_(mulib_sha_ctx *c)
{
  if(c->algorithm<=MULIB_SHA1) {
    uint32_t w[80], a=(uint32_t)c->state[0], b=(uint32_t)c->state[1],
      d=(uint32_t)c->state[3], e=(uint32_t)c->state[4], cc=(uint32_t)c->state[2];
    for(unsigned i=0;i<16;++i) w[i]=mulib_hc_be32(c->block+4*i);
    for(unsigned i=16;i<80;++i) {
      w[i]=w[i-3]^w[i-8]^w[i-14]^w[i-16];
      if(c->algorithm==MULIB_SHA1) w[i]=mulib_hc_r32(w[i],31);
    }
    for(unsigned i=0;i<80;++i) {
      uint32_t f,k,t;
      if(i<20) { f=(b&cc)|(~b&d); k=0x5a827999; }
      else if(i<40) { f=b^cc^d; k=0x6ed9eba1; }
      else if(i<60) { f=(b&cc)|(b&d)|(cc&d); k=0x8f1bbcdc; }
      else { f=b^cc^d; k=0xca62c1d6; }
      t=mulib_hc_r32(a,27)+f+e+k+w[i]; e=d; d=cc;
      cc=mulib_hc_r32(b,2); b=a; a=t;
    }
    c->state[0]=(uint32_t)(c->state[0]+a); c->state[1]=(uint32_t)(c->state[1]+b);
    c->state[2]=(uint32_t)(c->state[2]+cc); c->state[3]=(uint32_t)(c->state[3]+d);
    c->state[4]=(uint32_t)(c->state[4]+e);
  } else if(c->algorithm<=MULIB_SHA256) {
    uint32_t w[64], v[8];
    for(unsigned i=0;i<8;++i) v[i]=(uint32_t)c->state[i];
    for(unsigned i=0;i<16;++i) w[i]=mulib_hc_be32(c->block+4*i);
    for(unsigned i=16;i<64;i+=4) {
#if MULIB_HASH_SSE2
      if(c->intrinsics) {
        __m128i x=_mm_loadu_si128((const __m128i *)(const void *)(w+i-15));
        __m128i r7=_mm_or_si128(_mm_srli_epi32(x,7),_mm_slli_epi32(x,25));
        __m128i r18=_mm_or_si128(_mm_srli_epi32(x,18),_mm_slli_epi32(x,14));
        x=_mm_xor_si128(_mm_xor_si128(r7,r18),_mm_srli_epi32(x,3));
        x=_mm_add_epi32(x,_mm_loadu_si128((const __m128i *)(const void *)(w+i-16)));
        _mm_storeu_si128((__m128i *)(void *)(w+i),x);
        for(unsigned j=i;j<i+4;++j) {
          uint32_t y=w[j-2];
          w[j]+=w[j-7]+(mulib_hc_r32(y,17)^mulib_hc_r32(y,19)^(y>>10));
        }
      } else
#endif
      for(unsigned j=i;j<i+4;++j) {
        uint32_t x=w[j-15], y=w[j-2];
        w[j]=w[j-16]+w[j-7]+(mulib_hc_r32(x,7)^mulib_hc_r32(x,18)^(x>>3))
          +(mulib_hc_r32(y,17)^mulib_hc_r32(y,19)^(y>>10));
      }
    }
    for(unsigned i=0;i<64;++i) {
      uint32_t a=v[0],b=v[1],cc=v[2],e=v[4],f=v[5],g=v[6];
      uint32_t t=v[7]+(mulib_hc_r32(e,6)^mulib_hc_r32(e,11)^mulib_hc_r32(e,25))
        +((e&f)^(~e&g))+mulib_sha_k32_[i]+w[i];
      uint32_t u=(mulib_hc_r32(a,2)^mulib_hc_r32(a,13)^mulib_hc_r32(a,22))
        +((a&b)^(a&cc)^(b&cc));
      for(unsigned j=7;j>0;--j) v[j]=v[j-1];
      v[4]+=t; v[0]=t+u;
    }
    for(unsigned i=0;i<8;++i) c->state[i]=(uint32_t)(c->state[i]+v[i]);
  } else {
    uint64_t w[80],v[8]; memcpy(v,c->state,sizeof v);
    for(unsigned i=0;i<16;++i) w[i]=mulib_hc_be64(c->block+8*i);
    for(unsigned i=16;i<80;i+=2) {
#if MULIB_HASH_SSE2
      if(c->intrinsics) {
        __m128i x=_mm_loadu_si128((const __m128i *)(const void *)(w+i-15));
        __m128i r1=_mm_or_si128(_mm_srli_epi64(x,1),_mm_slli_epi64(x,63));
        __m128i r8=_mm_or_si128(_mm_srli_epi64(x,8),_mm_slli_epi64(x,56));
        x=_mm_xor_si128(_mm_xor_si128(r1,r8),_mm_srli_epi64(x,7));
        x=_mm_add_epi64(x,_mm_loadu_si128((const __m128i *)(const void *)(w+i-16)));
        x=_mm_add_epi64(x,_mm_loadu_si128((const __m128i *)(const void *)(w+i-7)));
        _mm_storeu_si128((__m128i *)(void *)(w+i),x);
        for(unsigned j=i;j<i+2;++j) {
          uint64_t y=w[j-2]; w[j]+=mulib_hc_r64(y,19)^mulib_hc_r64(y,61)^(y>>6);
        }
      } else
#endif
      for(unsigned j=i;j<i+2;++j) {
        uint64_t x=w[j-15], y=w[j-2];
        w[j]=w[j-16]+w[j-7]+(mulib_hc_r64(x,1)^mulib_hc_r64(x,8)^(x>>7))
          +(mulib_hc_r64(y,19)^mulib_hc_r64(y,61)^(y>>6));
      }
    }
    for(unsigned i=0;i<80;++i) {
      uint64_t a=v[0],b=v[1],cc=v[2],e=v[4],f=v[5],g=v[6];
      uint64_t t=v[7]+(mulib_hc_r64(e,14)^mulib_hc_r64(e,18)^mulib_hc_r64(e,41))
        +((e&f)^(~e&g))+mulib_sha_k64_[i]+w[i];
      uint64_t u=(mulib_hc_r64(a,28)^mulib_hc_r64(a,34)^mulib_hc_r64(a,39))
        +((a&b)^(a&cc)^(b&cc));
      for(unsigned j=7;j>0;--j) v[j]=v[j-1];
      v[4]+=t; v[0]=t+u;
    }
    for(unsigned i=0;i<8;++i) c->state[i]+=v[i];
  }
}
static void mulib_sha_keccak_(uint64_t a[25],unsigned simd)
{
  static const uint64_t rc[24]={
    0x1,0x8082,UINT64_C(0x800000000000808a),UINT64_C(0x8000000080008000),
    0x808b,0x80000001,UINT64_C(0x8000000080008081),UINT64_C(0x8000000000008009),
    0x8a,0x88,0x80008009,0x8000000a,0x8000808b,UINT64_C(0x800000000000008b),
    UINT64_C(0x8000000000008089),UINT64_C(0x8000000000008003),
    UINT64_C(0x8000000000008002),UINT64_C(0x8000000000000080),
    0x800a,UINT64_C(0x800000008000000a),UINT64_C(0x8000000080008081),
    UINT64_C(0x8000000000008080),0x80000001,UINT64_C(0x8000000080008008)};
  static const unsigned rot[25]={0,1,62,28,27,36,44,6,55,20,3,10,43,25,39,41,45,15,21,8,18,2,61,56,14};
  for(unsigned round=0;round<24;++round) {
    uint64_t b[25],cc[5],d[5];
    for(unsigned x=0;x<5;++x) cc[x]=a[x]^a[x+5]^a[x+10]^a[x+15]^a[x+20];
    for(unsigned x=0;x<5;++x) d[x]=cc[(x+4)%5]^mulib_hc_r64(cc[(x+1)%5],63);
#if MULIB_HASH_SSE2
    if(simd) {
      for(unsigned i=0;i<24;i+=2) {
        __m128i x=_mm_loadu_si128((const __m128i *)(const void *)(a+i));
        __m128i y=_mm_set_epi64x((long long)d[(i+1)%5],(long long)d[i%5]);
        _mm_storeu_si128((__m128i *)(void *)(a+i),_mm_xor_si128(x,y));
      }
      a[24]^=d[4];
    } else
#else
    (void)simd;
#endif
    for(unsigned i=0;i<25;++i) a[i]^=d[i%5];
    for(unsigned y=0;y<5;++y) for(unsigned x=0;x<5;++x) {
      unsigned i=x+5*y;
      b[y+5*((2*x+3*y)%5)]=mulib_hc_r64(a[i],64-rot[i]);
    }
#if MULIB_HASH_SSE2
    if(simd) {
      for(unsigned y=0;y<5;++y) {
        unsigned row=5*y;
        for(unsigned x=0;x<4;x+=2) {
          __m128i v=_mm_loadu_si128((const __m128i *)(const void *)(b+row+x));
          __m128i u=_mm_set_epi64x((long long)b[row+(x+2)%5],(long long)b[row+(x+1)%5]);
          __m128i w=_mm_set_epi64x((long long)b[row+(x+3)%5],(long long)b[row+(x+2)%5]);
          _mm_storeu_si128((__m128i *)(void *)(a+row+x),_mm_xor_si128(v,_mm_andnot_si128(u,w)));
        }
        a[row+4]=b[row+4]^((~b[row])&b[row+1]);
      }
    } else
#endif
    for(unsigned y=0;y<5;++y) for(unsigned x=0;x<5;++x)
      a[x+5*y]=b[x+5*y]^((~b[(x+1)%5+5*y])&b[(x+2)%5+5*y]);
    a[0]^=rc[round];
  }
}
size_t mulib_sha_digest_size(mulib_sha_algorithm a)
{
  static const unsigned sizes[]={20,20,28,32,48,64,28,32,28,32,48,64,0,0};
  return (unsigned)a<sizeof sizes/sizeof sizes[0] ? sizes[a] : 0;
}
int mulib_sha_init(mulib_sha_ctx *c, mulib_sha_algorithm a)
{
  static const uint64_t iv[8][8]={
    {0x67452301,0xefcdab89,0x98badcfe,0x10325476,0xc3d2e1f0},
    {0x67452301,0xefcdab89,0x98badcfe,0x10325476,0xc3d2e1f0},
    {0xc1059ed8,0x367cd507,0x3070dd17,0xf70e5939,0xffc00b31,0x68581511,0x64f98fa7,0xbefa4fa4},
    {0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19},
    {UINT64_C(0xcbbb9d5dc1059ed8),UINT64_C(0x629a292a367cd507),UINT64_C(0x9159015a3070dd17),UINT64_C(0x152fecd8f70e5939),UINT64_C(0x67332667ffc00b31),UINT64_C(0x8eb44a8768581511),UINT64_C(0xdb0c2e0d64f98fa7),UINT64_C(0x47b5481dbefa4fa4)},
    {UINT64_C(0x6a09e667f3bcc908),UINT64_C(0xbb67ae8584caa73b),UINT64_C(0x3c6ef372fe94f82b),UINT64_C(0xa54ff53a5f1d36f1),UINT64_C(0x510e527fade682d1),UINT64_C(0x9b05688c2b3e6c1f),UINT64_C(0x1f83d9abfb41bd6b),UINT64_C(0x5be0cd19137e2179)},
    {UINT64_C(0x8c3d37c819544da2),UINT64_C(0x73e1996689dcd4d6),UINT64_C(0x1dfab7ae32ff9c82),UINT64_C(0x679dd514582f9fcf),UINT64_C(0x0f6d2b697bd44da8),UINT64_C(0x77e36f7304c48942),UINT64_C(0x3f9d85a86a1d36c8),UINT64_C(0x1112e6ad91d692a1)},
    {UINT64_C(0x22312194fc2bf72c),UINT64_C(0x9f555fa3c84c64c2),UINT64_C(0x2393b86b6f53b151),UINT64_C(0x963877195940eabd),UINT64_C(0x96283ee2a88effe3),UINT64_C(0xbe5e1e2553863992),UINT64_C(0x2b0199fc2c85b8aa),UINT64_C(0x0eb72ddc81c52ca2)}};
  if(!c || (unsigned)a>MULIB_SHAKE256) return 0;
  memset(c,0,sizeof *c); c->algorithm=a; c->phase=1;
  c->digest_size=mulib_sha_digest_size(a);
  if(a<MULIB_SHA3_224) {
    memcpy(c->state,iv[a],sizeof iv[a]); c->rate=a<=MULIB_SHA256 ? 64 : 128;
  } else {
    c->rate=a==MULIB_SHAKE128 ? 168 : a==MULIB_SHAKE256 ? 136 : 200-2*c->digest_size;
    c->domain=a>=MULIB_SHAKE128 ? 0x1f : 6;
  }
  return 1;
}
int mulib_sha_init_intrinsics(mulib_sha_ctx *c, mulib_sha_algorithm a)
{ if(!mulib_sha_init(c,a)) return 0; c->intrinsics=MULIB_HASH_SSE2; return 1; }
int mulib_sha_update(mulib_sha_ctx *c, const void *data, size_t n)
{
  const unsigned char *p=(const unsigned char *)data;
  uint64_t lo,hi;
  if(!c || c->phase!=1 || (!p && n)) return 0;
  lo=c->bytes_lo+(uint64_t)n; hi=c->bytes_hi+(lo<c->bytes_lo);
  if(hi<c->bytes_hi || (c->algorithm<=MULIB_SHA256 && (hi || lo>UINT64_MAX/8))
     || (c->algorithm<MULIB_SHA3_224 && hi>UINT64_MAX/8)) return 0;
  c->bytes_lo=lo; c->bytes_hi=hi;
  if(c->algorithm>=MULIB_SHA3_224) {
    while(n--) {
      c->state[c->used/8]^=(uint64_t)*p++ << (8*(c->used%8));
      if(++c->used==c->rate) { mulib_sha_keccak_(c->state,c->intrinsics); c->used=0; }
    }
  } else while(n) {
    size_t k=c->rate-c->used; if(k>n) k=n;
    memcpy(c->block+c->used,p,k); c->used+=k; p+=k; n-=k;
    if(c->used==c->rate) { mulib_sha_compress_(c); c->used=0; }
  }
  return 1;
}
int mulib_sha_squeeze(mulib_sha_ctx *c, void *out, size_t n)
{
  unsigned char *p=(unsigned char *)out;
  if(!c || c->algorithm<MULIB_SHA3_224 || (c->phase!=1 && c->phase!=2) || (!p && n)) return 0;
  if(c->phase==1) {
    c->state[c->used/8]^=(uint64_t)c->domain << (8*(c->used%8));
    c->state[(c->rate-1)/8]^=UINT64_C(0x80) << (8*((c->rate-1)%8));
    mulib_sha_keccak_(c->state,c->intrinsics); c->used=0; c->phase=2;
  }
  while(n--) {
    if(c->used==c->rate) { mulib_sha_keccak_(c->state,c->intrinsics); c->used=0; }
    *p++=(unsigned char)(c->state[c->used/8] >> (8*(c->used%8))); ++c->used;
  }
  return 1;
}
int mulib_sha_final(mulib_sha_ctx *c, void *out, size_t capacity)
{
  unsigned char *p=(unsigned char *)out;
  if(!c || c->phase!=1 || !c->digest_size || !p || capacity<c->digest_size) return 0;
  if(c->algorithm>=MULIB_SHA3_224) mulib_sha_squeeze(c,p,c->digest_size);
  else {
    size_t lenbytes=c->rate==128 ? 16 : 8;
    c->block[c->used++]=0x80;
    if(c->used>c->rate-lenbytes) {
      memset(c->block+c->used,0,c->rate-c->used); mulib_sha_compress_(c); c->used=0;
    }
    memset(c->block+c->used,0,c->rate-c->used);
    if(lenbytes==16) mulib_hc_putbe(c->block+c->rate-16,(c->bytes_hi<<3)|(c->bytes_lo>>61),8);
    mulib_hc_putbe(c->block+c->rate-8,c->bytes_lo<<3,8); mulib_sha_compress_(c);
    for(size_t i=0;i<c->digest_size;++i) {
      size_t word=c->rate==128 ? 8 : 4;
      p[i]=(unsigned char)(c->state[i/word] >> (8*(word-1-i%word)));
    }
  }
  mulib_hc_wipe(c,sizeof *c); return 1;
}
static int mulib_sha_once_(mulib_sha_algorithm a,const void *p,size_t n,void *out,size_t cap,int simd)
{
  mulib_sha_ctx c; int ok;
  if(!out && cap) return 0;
  if(!(simd ? mulib_sha_init_intrinsics(&c,a) : mulib_sha_init(&c,a))) return 0;
  ok=mulib_sha_update(&c,p,n);
  if(ok) ok=a>=MULIB_SHAKE128 ? mulib_sha_squeeze(&c,out,cap) : mulib_sha_final(&c,out,cap);
  mulib_hc_wipe(&c,sizeof c); return ok;
}
int mulib_sha(mulib_sha_algorithm a,const void *p,size_t n,void *out,size_t cap)
{ return mulib_sha_once_(a,p,n,out,cap,0); }
int mulib_sha_intrinsics(mulib_sha_algorithm a,const void *p,size_t n,void *out,size_t cap)
{ return mulib_sha_once_(a,p,n,out,cap,1); }
int mulib_sha0(const void *p,size_t n,unsigned char out[20])
{ return mulib_sha(MULIB_SHA0,p,n,out,20); }
int mulib_sha1(const void *p,size_t n,unsigned char out[20])
{ return mulib_sha(MULIB_SHA1,p,n,out,20); }
int mulib_sha224(const void *p,size_t n,unsigned char out[28])
{ return mulib_sha(MULIB_SHA224,p,n,out,28); }
int mulib_sha256(const void *p,size_t n,unsigned char out[32])
{ return mulib_sha(MULIB_SHA256,p,n,out,32); }
int mulib_sha384(const void *p,size_t n,unsigned char out[48])
{ return mulib_sha(MULIB_SHA384,p,n,out,48); }
int mulib_sha512(const void *p,size_t n,unsigned char out[64])
{ return mulib_sha(MULIB_SHA512,p,n,out,64); }
int mulib_sha512_224(const void *p,size_t n,unsigned char out[28])
{ return mulib_sha(MULIB_SHA512_224,p,n,out,28); }
int mulib_sha512_256(const void *p,size_t n,unsigned char out[32])
{ return mulib_sha(MULIB_SHA512_256,p,n,out,32); }
int mulib_sha3_224(const void *p,size_t n,unsigned char out[28])
{ return mulib_sha(MULIB_SHA3_224,p,n,out,28); }
int mulib_sha3_256(const void *p,size_t n,unsigned char out[32])
{ return mulib_sha(MULIB_SHA3_256,p,n,out,32); }
int mulib_sha3_384(const void *p,size_t n,unsigned char out[48])
{ return mulib_sha(MULIB_SHA3_384,p,n,out,48); }
int mulib_sha3_512(const void *p,size_t n,unsigned char out[64])
{ return mulib_sha(MULIB_SHA3_512,p,n,out,64); }
int mulib_shake128(const void *p,size_t n,void *out,size_t cap)
{ return mulib_sha(MULIB_SHAKE128,p,n,out,cap); }
int mulib_shake256(const void *p,size_t n,void *out,size_t cap)
{ return mulib_sha(MULIB_SHAKE256,p,n,out,cap); }
#ifdef __cplusplus
}
#endif
#endif
