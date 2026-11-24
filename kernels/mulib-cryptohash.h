/** @file mulib-cryptohash.h
 * @brief BLAKE2, BLAKE3, KMAC, SipHash and AES-PMAC with intrinsic alternatives.
 * BLAKE2b/s (including keyed mode), BLAKE3 (hash, keyed, derive-key, XOF),
 * SipHash-2-4/1-3, KMAC128/256/XOF and AES-128/192/256 PMAC (original PMAC1).
 * Define MULIB_IMPL_CRYPTOHASH once. KMAC also requires MULIB_IMPL_SHA in one
 * translation unit. No allocation. All lengths are bytes; output must not
 * overlap inputs/context. Status functions return 1/0. NULL is valid only for
 * zero-length buffers. Fixed-size keys and output pointers must be non-NULL.
 * BLAKE2 final consumes/wipes state; BLAKE3 final is a repeatable snapshot.
 * Contexts contain secrets; wipe abandoned contexts with mulib_cryptohash_wipe.
 * _intrinsics variants use compiled SSE2 / AES-NI code, with portable fallbacks.
 * AES-NI requires compilation with -maes and a supporting CPU. Define
 * MULIB_HASH_NO_INTRINSICS to force portable code. Portable AES uses a constant
 * operation count algebraic S-box, without secret-indexed lookup tables.
 * SipHash is a keyed short-input PRF; PMAC/KMAC are MACs, not unkeyed digests.
 */
#ifndef MULIB_CRYPTOHASH_H
#define MULIB_CRYPTOHASH_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-sha.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
  uint64_t h[8], counter[2];
  unsigned char block[128];
  size_t used, out_length;
  unsigned wide, active, intrinsics;
} mulib_blake2_ctx;
int mulib_blake2s_init(mulib_blake2_ctx *ctx,size_t out_length,const void *key,size_t key_length);
int mulib_blake2b_init(mulib_blake2_ctx *ctx,size_t out_length,const void *key,size_t key_length);
int mulib_blake2s_init_intrinsics(mulib_blake2_ctx *ctx,size_t out_length,const void *key,size_t key_length);
int mulib_blake2b_init_intrinsics(mulib_blake2_ctx *ctx,size_t out_length,const void *key,size_t key_length);
int mulib_blake2_update(mulib_blake2_ctx *ctx,const void *data,size_t length);
int mulib_blake2_final(mulib_blake2_ctx *ctx,void *out,size_t capacity);
int mulib_blake2s(const void *data,size_t length,const void *key,size_t key_length,void *out,size_t out_length);
int mulib_blake2b(const void *data,size_t length,const void *key,size_t key_length,void *out,size_t out_length);
int mulib_blake2s_intrinsics(const void *data,size_t length,const void *key,size_t key_length,void *out,size_t out_length);
int mulib_blake2b_intrinsics(const void *data,size_t length,const void *key,size_t key_length,void *out,size_t out_length);

typedef struct {
  uint32_t key[8], cv[8], stack[54][8];
  uint64_t chunk, total;
  unsigned char block[64];
  unsigned used, blocks, stack_length, flags, active, intrinsics;
} mulib_blake3_ctx;
int mulib_blake3_init(mulib_blake3_ctx *ctx);
int mulib_blake3_init_keyed(mulib_blake3_ctx *ctx,const unsigned char key[32]);
int mulib_blake3_init_derive_key(mulib_blake3_ctx *ctx,const void *context,size_t context_length);
int mulib_blake3_init_intrinsics(mulib_blake3_ctx *ctx);
int mulib_blake3_init_keyed_intrinsics(mulib_blake3_ctx *ctx,const unsigned char key[32]);
int mulib_blake3_init_derive_key_intrinsics(mulib_blake3_ctx *ctx,const void *context,size_t context_length);
int mulib_blake3_update(mulib_blake3_ctx *ctx,const void *data,size_t length);
int mulib_blake3_final(const mulib_blake3_ctx *ctx,void *out,size_t out_length);
int mulib_blake3_final_seek(const mulib_blake3_ctx *ctx,uint64_t seek,void *out,size_t out_length);
int mulib_blake3(const void *data,size_t length,void *out,size_t out_length);
int mulib_blake3_intrinsics(const void *data,size_t length,void *out,size_t out_length);
int mulib_blake3_keyed(const unsigned char key[32],const void *data,size_t length,void *out,size_t out_length);
int mulib_blake3_derive_key(const void *context,size_t context_length,const void *data,size_t length,void *out,size_t out_length);

int mulib_siphash24(const unsigned char key[16],const void *data,size_t length,unsigned char out[8]);
int mulib_siphash13(const unsigned char key[16],const void *data,size_t length,unsigned char out[8]);
/* KMAC XOF differs from fixed-output KMAC: right_encode(0) replaces L. */
int mulib_kmac128(const void *key,size_t key_length,const void *data,size_t length,
                  const void *custom,size_t custom_length,void *out,size_t out_length);
int mulib_kmac256(const void *key,size_t key_length,const void *data,size_t length,
                  const void *custom,size_t custom_length,void *out,size_t out_length);
int mulib_kmacxof128(const void *key,size_t key_length,const void *data,size_t length,
                     const void *custom,size_t custom_length,void *out,size_t out_length);
int mulib_kmacxof256(const void *key,size_t key_length,const void *data,size_t length,
                     const void *custom,size_t custom_length,void *out,size_t out_length);
int mulib_kmac128_intrinsics(const void *key,size_t key_length,const void *data,size_t length,
                  const void *custom,size_t custom_length,void *out,size_t out_length);
int mulib_kmac256_intrinsics(const void *key,size_t key_length,const void *data,size_t length,
                  const void *custom,size_t custom_length,void *out,size_t out_length);
int mulib_kmacxof128_intrinsics(const void *key,size_t key_length,const void *data,size_t length,
                  const void *custom,size_t custom_length,void *out,size_t out_length);
int mulib_kmacxof256_intrinsics(const void *key,size_t key_length,const void *data,size_t length,
                  const void *custom,size_t custom_length,void *out,size_t out_length);
/* key_length is 16, 24 or 32; tag is always 16 bytes. */
int mulib_pmac(const void *key,size_t key_length,const void *data,size_t length,unsigned char tag[16]);
int mulib_pmac_intrinsics(const void *key,size_t key_length,const void *data,size_t length,unsigned char tag[16]);
void mulib_cryptohash_wipe(void *data,size_t length);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CRYPTOHASH) && !defined(MULIB_CRYPTOHASH_IMPLEMENTED)
#define MULIB_CRYPTOHASH_IMPLEMENTED
#include "detail/mulib-hash-core.h"
#if defined(__AES__) && !defined(MULIB_HASH_NO_INTRINSICS)
#include <wmmintrin.h>
#endif
#ifdef __cplusplus
extern "C" {
#endif
void mulib_cryptohash_wipe(void *p,size_t n) { if(p) mulib_hc_wipe(p,n); }
static const uint32_t mulib_ch_iv32_[8]={
  0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
static const uint64_t mulib_ch_iv64_[8]={
  UINT64_C(0x6a09e667f3bcc908),UINT64_C(0xbb67ae8584caa73b),UINT64_C(0x3c6ef372fe94f82b),UINT64_C(0xa54ff53a5f1d36f1),
  UINT64_C(0x510e527fade682d1),UINT64_C(0x9b05688c2b3e6c1f),UINT64_C(0x1f83d9abfb41bd6b),UINT64_C(0x5be0cd19137e2179)};
static const unsigned char mulib_ch_sigma_[12][16]={
 {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15},
 {14,10,4,8,9,15,13,6,1,12,0,2,11,7,5,3},
 {11,8,12,0,5,2,15,13,10,14,3,6,7,1,9,4},
 {7,9,3,1,13,12,11,14,2,6,5,10,4,0,15,8},
 {9,0,5,7,2,4,10,15,14,1,11,12,6,8,3,13},
 {2,12,6,10,0,11,8,3,4,13,7,5,15,14,1,9},
 {12,5,1,15,14,13,4,10,0,7,6,3,9,2,8,11},
 {13,11,7,14,12,1,3,9,5,0,15,4,8,6,2,10},
 {6,15,14,9,11,3,0,8,12,2,13,7,1,4,10,5},
 {10,2,8,4,7,6,1,5,15,11,9,14,3,12,13,0},
 {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15},
 {14,10,4,8,9,15,13,6,1,12,0,2,11,7,5,3}};
static void mulib_ch_g32_(uint32_t v[16],unsigned a,unsigned b,unsigned c,unsigned d,uint32_t x,uint32_t y)
{
  v[a]+=v[b]+x; v[d]=mulib_hc_r32(v[d]^v[a],16);
  v[c]+=v[d]; v[b]=mulib_hc_r32(v[b]^v[c],12);
  v[a]+=v[b]+y; v[d]=mulib_hc_r32(v[d]^v[a],8);
  v[c]+=v[d]; v[b]=mulib_hc_r32(v[b]^v[c],7);
}
#if MULIB_HASH_SSE2
#define MULIB_CH_ROR32_(v,n) _mm_or_si128(_mm_srli_epi32(v,n),_mm_slli_epi32(v,32-n))
#define MULIB_CH_VG32_(a,b,c,d,x,y) do { \
  a=_mm_add_epi32(_mm_add_epi32(a,b),x); d=_mm_xor_si128(d,a); d=MULIB_CH_ROR32_(d,16); \
  c=_mm_add_epi32(c,d); b=_mm_xor_si128(b,c); b=MULIB_CH_ROR32_(b,12); \
  a=_mm_add_epi32(_mm_add_epi32(a,b),y); d=_mm_xor_si128(d,a); d=MULIB_CH_ROR32_(d,8); \
  c=_mm_add_epi32(c,d); b=_mm_xor_si128(b,c); b=MULIB_CH_ROR32_(b,7); \
} while(0)
static void mulib_ch_round32_simd_(uint32_t v[16],const uint32_t m[16],const unsigned char s[16])
{
  __m128i a=_mm_loadu_si128((const __m128i *)(const void *)v);
  __m128i b=_mm_loadu_si128((const __m128i *)(const void *)(v+4));
  __m128i c=_mm_loadu_si128((const __m128i *)(const void *)(v+8));
  __m128i d=_mm_loadu_si128((const __m128i *)(const void *)(v+12));
  __m128i x=_mm_setr_epi32((int)m[s[0]],(int)m[s[2]],(int)m[s[4]],(int)m[s[6]]);
  __m128i y=_mm_setr_epi32((int)m[s[1]],(int)m[s[3]],(int)m[s[5]],(int)m[s[7]]);
  MULIB_CH_VG32_(a,b,c,d,x,y);
  b=_mm_shuffle_epi32(b,_MM_SHUFFLE(0,3,2,1)); c=_mm_shuffle_epi32(c,_MM_SHUFFLE(1,0,3,2)); d=_mm_shuffle_epi32(d,_MM_SHUFFLE(2,1,0,3));
  x=_mm_setr_epi32((int)m[s[8]],(int)m[s[10]],(int)m[s[12]],(int)m[s[14]]);
  y=_mm_setr_epi32((int)m[s[9]],(int)m[s[11]],(int)m[s[13]],(int)m[s[15]]);
  MULIB_CH_VG32_(a,b,c,d,x,y);
  b=_mm_shuffle_epi32(b,_MM_SHUFFLE(2,1,0,3)); c=_mm_shuffle_epi32(c,_MM_SHUFFLE(1,0,3,2)); d=_mm_shuffle_epi32(d,_MM_SHUFFLE(0,3,2,1));
  _mm_storeu_si128((__m128i *)(void *)v,a); _mm_storeu_si128((__m128i *)(void *)(v+4),b);
  _mm_storeu_si128((__m128i *)(void *)(v+8),c); _mm_storeu_si128((__m128i *)(void *)(v+12),d);
}
#undef MULIB_CH_VG32_
#undef MULIB_CH_ROR32_
#endif
static void mulib_ch_round32_(uint32_t v[16],const uint32_t m[16],const unsigned char s[16],unsigned simd)
{
#if MULIB_HASH_SSE2
  if(simd) { mulib_ch_round32_simd_(v,m,s); return; }
#else
  (void)simd;
#endif
  for(unsigned i=0;i<4;++i) mulib_ch_g32_(v,i,i+4,i+8,i+12,m[s[2*i]],m[s[2*i+1]]);
  for(unsigned i=0;i<4;++i) mulib_ch_g32_(v,i,4+(i+1)%4,8+(i+2)%4,12+(i+3)%4,m[s[8+2*i]],m[s[9+2*i]]);
}
/* Two independent 64-bit G operations per SSE2 register. */
static void mulib_ch_g64pair_(uint64_t v[16],const uint64_t m[16],const unsigned char s[16],unsigned i,unsigned diagonal,unsigned simd)
{
  unsigned a[2],b[2],c[2],d[2],ix[2],iy[2];
  for(unsigned j=0;j<2;++j) {
    unsigned k=i+j; a[j]=k; b[j]=4+(k+(diagonal ? 1 : 0))%4;
    c[j]=8+(k+(diagonal ? 2 : 0))%4; d[j]=12+(k+(diagonal ? 3 : 0))%4;
    ix[j]=s[diagonal*8+2*k]; iy[j]=s[diagonal*8+2*k+1];
  }
#if MULIB_HASH_SSE2
  if(simd) {
    __m128i va=_mm_set_epi64x((long long)v[a[1]],(long long)v[a[0]]);
    __m128i vb=_mm_set_epi64x((long long)v[b[1]],(long long)v[b[0]]);
    __m128i vc=_mm_set_epi64x((long long)v[c[1]],(long long)v[c[0]]);
    __m128i vd=_mm_set_epi64x((long long)v[d[1]],(long long)v[d[0]]);
    __m128i x=_mm_set_epi64x((long long)m[ix[1]],(long long)m[ix[0]]);
    __m128i y=_mm_set_epi64x((long long)m[iy[1]],(long long)m[iy[0]]);
    uint64_t tmp[2];
#define MULIB_CH_R64_(z,n) _mm_or_si128(_mm_srli_epi64(z,n),_mm_slli_epi64(z,64-n))
    va=_mm_add_epi64(_mm_add_epi64(va,vb),x); vd=_mm_xor_si128(vd,va); vd=MULIB_CH_R64_(vd,32);
    vc=_mm_add_epi64(vc,vd); vb=_mm_xor_si128(vb,vc); vb=MULIB_CH_R64_(vb,24);
    va=_mm_add_epi64(_mm_add_epi64(va,vb),y); vd=_mm_xor_si128(vd,va); vd=MULIB_CH_R64_(vd,16);
    vc=_mm_add_epi64(vc,vd); vb=_mm_xor_si128(vb,vc); vb=MULIB_CH_R64_(vb,63);
#undef MULIB_CH_R64_
#define MULIB_CH_SAVE_(reg,idx) do { _mm_storeu_si128((__m128i *)(void *)tmp,reg); v[idx[0]]=tmp[0]; v[idx[1]]=tmp[1]; } while(0)
    MULIB_CH_SAVE_(va,a); MULIB_CH_SAVE_(vb,b); MULIB_CH_SAVE_(vc,c); MULIB_CH_SAVE_(vd,d);
#undef MULIB_CH_SAVE_
    return;
  }
#else
  (void)simd;
#endif
  for(unsigned j=0;j<2;++j) {
    unsigned aa=a[j],bb=b[j],cc=c[j],dd=d[j];
    v[aa]+=v[bb]+m[ix[j]]; v[dd]=mulib_hc_r64(v[dd]^v[aa],32);
    v[cc]+=v[dd]; v[bb]=mulib_hc_r64(v[bb]^v[cc],24);
    v[aa]+=v[bb]+m[iy[j]]; v[dd]=mulib_hc_r64(v[dd]^v[aa],16);
    v[cc]+=v[dd]; v[bb]=mulib_hc_r64(v[bb]^v[cc],63);
  }
}
static void mulib_blake2_compress_(mulib_blake2_ctx *c,unsigned last,size_t bytes)
{
  uint64_t old=c->counter[0]; c->counter[0]+=bytes;
  c->counter[1]+=c->counter[0]<old;
  if(c->wide) {
    uint64_t v[16],m[16];
    for(unsigned i=0;i<8;++i) { v[i]=c->h[i]; v[i+8]=mulib_ch_iv64_[i]; }
    for(unsigned i=0;i<16;++i) m[i]=mulib_hc_le64(c->block+8*i);
    v[12]^=c->counter[0]; v[13]^=c->counter[1]; if(last) v[14]=~v[14];
    for(unsigned r=0;r<12;++r) for(unsigned d=0;d<2;++d) for(unsigned i=0;i<4;i+=2)
      mulib_ch_g64pair_(v,m,mulib_ch_sigma_[r],i,d,c->intrinsics);
    for(unsigned i=0;i<8;++i) c->h[i]^=v[i]^v[i+8];
    mulib_hc_wipe(v,sizeof v); mulib_hc_wipe(m,sizeof m);
  } else {
    uint32_t v[16],m[16];
    for(unsigned i=0;i<8;++i) { v[i]=(uint32_t)c->h[i]; v[i+8]=mulib_ch_iv32_[i]; }
    for(unsigned i=0;i<16;++i) m[i]=mulib_hc_le32(c->block+4*i);
    v[12]^=(uint32_t)c->counter[0]; v[13]^=(uint32_t)(c->counter[0]>>32); if(last) v[14]=~v[14];
    for(unsigned r=0;r<10;++r) mulib_ch_round32_(v,m,mulib_ch_sigma_[r],c->intrinsics);
    for(unsigned i=0;i<8;++i) c->h[i]=(uint32_t)c->h[i]^v[i]^v[i+8];
    mulib_hc_wipe(v,sizeof v); mulib_hc_wipe(m,sizeof m);
  }
}
static int mulib_blake2_init_(mulib_blake2_ctx *c,size_t out,const void *key,size_t kn,unsigned wide)
{
  size_t max=wide ? 64 : 32;
  if(!c || !out || out>max || kn>max || (!key && kn)) return 0;
  memset(c,0,sizeof *c); c->wide=wide; c->active=1; c->out_length=out;
  for(unsigned i=0;i<8;++i) c->h[i]=wide ? mulib_ch_iv64_[i] : mulib_ch_iv32_[i];
  c->h[0]^=UINT32_C(0x01010000)|((uint32_t)kn<<8)|(uint32_t)out;
  if(kn) { memcpy(c->block,key,kn); c->used=wide ? 128 : 64; }
  return 1;
}
int mulib_blake2s_init(mulib_blake2_ctx *c,size_t out,const void *k,size_t kn)
{ return mulib_blake2_init_(c,out,k,kn,0); }
int mulib_blake2b_init(mulib_blake2_ctx *c,size_t out,const void *k,size_t kn)
{ return mulib_blake2_init_(c,out,k,kn,1); }
int mulib_blake2s_init_intrinsics(mulib_blake2_ctx *c,size_t on,const void *k,size_t kn)
{ if(!mulib_blake2s_init(c,on,k,kn)) return 0; c->intrinsics=MULIB_HASH_SSE2; return 1; }
int mulib_blake2b_init_intrinsics(mulib_blake2_ctx *c,size_t on,const void *k,size_t kn)
{ if(!mulib_blake2b_init(c,on,k,kn)) return 0; c->intrinsics=MULIB_HASH_SSE2; return 1; }
int mulib_blake2_update(mulib_blake2_ctx *c,const void *data,size_t n)
{
  const unsigned char *p=(const unsigned char *)data;
  if(!c || c->active!=1 || (!p && n)) return 0;
  /* BLAKE2s has a 64-bit byte counter. BLAKE2b has a 128-bit counter. */
  if(!c->wide && (c->counter[0]>UINT64_MAX-c->used || n>UINT64_MAX-c->counter[0]-c->used)) return 0;
  if(c->wide) {
    uint64_t lo=c->counter[0]+(uint64_t)c->used;
    uint64_t carry=lo<c->counter[0],hi=c->counter[1]+carry;
    if(hi<c->counter[1] || ((uint64_t)n>UINT64_MAX-lo && hi==UINT64_MAX)) return 0;
  }
  while(n) {
    size_t rate=c->wide ? 128 : 64,k;
    if(c->used==rate) { mulib_blake2_compress_(c,0,rate); c->used=0; }
    k=rate-c->used; if(k>n) k=n;
    memcpy(c->block+c->used,p,k); c->used+=k; p+=k; n-=k;
  }
  return 1;
}
int mulib_blake2_final(mulib_blake2_ctx *c,void *out,size_t cap)
{
  unsigned char bytes[64];
  if(!c || c->active!=1 || !out || cap<c->out_length) return 0;
  memset(c->block+c->used,0,(c->wide ? 128 : 64)-c->used);
  mulib_blake2_compress_(c,1,c->used);
  for(unsigned i=0;i<8;++i) mulib_hc_putle(bytes+i*(c->wide ? 8 : 4),c->h[i],c->wide ? 8 : 4);
  memcpy(out,bytes,c->out_length); mulib_hc_wipe(bytes,sizeof bytes); mulib_hc_wipe(c,sizeof *c); return 1;
}
static int mulib_blake2_once_(const void *p,size_t n,const void *key,size_t kn,void *out,size_t on,unsigned wide,unsigned simd)
{
  mulib_blake2_ctx c; int ok;
  if(!out || !mulib_blake2_init_(&c,on,key,kn,wide)) return 0;
  c.intrinsics=simd; ok=mulib_blake2_update(&c,p,n) && mulib_blake2_final(&c,out,on);
  mulib_hc_wipe(&c,sizeof c); return ok;
}
#define MULIB_CH_B2_(name,wide,simd) \
int name(const void *p,size_t n,const void *k,size_t kn,void *out,size_t on) \
{ return mulib_blake2_once_(p,n,k,kn,out,on,wide,simd); }
MULIB_CH_B2_(mulib_blake2s,0,0)
MULIB_CH_B2_(mulib_blake2b,1,0)
MULIB_CH_B2_(mulib_blake2s_intrinsics,0,1)
MULIB_CH_B2_(mulib_blake2b_intrinsics,1,1)
#undef MULIB_CH_B2_
typedef struct {
  uint32_t cv[8], block[16]; uint64_t counter;
  unsigned length,flags;
} mulib_ch_b3output_;
static void mulib_blake3_compress_(const mulib_ch_b3output_ *o,uint64_t counter,unsigned flags,uint32_t out[16],unsigned simd)
{
  static const unsigned char perm[16]={2,6,3,10,7,0,4,13,1,11,12,5,9,14,15,8};
  uint32_t v[16]; unsigned char schedule[16],next[16];
  memcpy(v,o->cv,32); memcpy(v+8,mulib_ch_iv32_,16);
  v[12]=(uint32_t)counter; v[13]=(uint32_t)(counter>>32); v[14]=o->length; v[15]=flags;
  for(unsigned i=0;i<16;++i) schedule[i]=(unsigned char)i;
  for(unsigned r=0;r<7;++r) {
    mulib_ch_round32_(v,o->block,schedule,simd);
    for(unsigned i=0;i<16;++i) next[i]=schedule[perm[i]];
    memcpy(schedule,next,16);
  }
  for(unsigned i=0;i<8;++i) { out[i]=v[i]^v[i+8]; out[i+8]=v[i+8]^o->cv[i]; }
  mulib_hc_wipe(v,sizeof v);
}
static void mulib_blake3_cv_(const mulib_ch_b3output_ *o,uint32_t cv[8],unsigned simd)
{
  uint32_t words[16]; mulib_blake3_compress_(o,o->counter,o->flags,words,simd);
  memcpy(cv,words,32); mulib_hc_wipe(words,sizeof words);
}
static mulib_ch_b3output_ mulib_blake3_chunk_(const mulib_blake3_ctx *c)
{
  mulib_ch_b3output_ o;
  memcpy(o.cv,c->cv,32);
  for(unsigned i=0;i<16;++i) o.block[i]=mulib_hc_le32(c->block+4*i);
  o.counter=c->chunk; o.length=c->used; o.flags=c->flags|2|(c->blocks ? 0 : 1);
  return o;
}
static mulib_ch_b3output_ mulib_blake3_parent_(const mulib_blake3_ctx *c,const uint32_t left[8],const uint32_t right[8])
{
  mulib_ch_b3output_ o;
  memcpy(o.cv,c->key,32); memcpy(o.block,left,32); memcpy(o.block+8,right,32);
  o.counter=0; o.length=64; o.flags=c->flags|4; return o;
}
int mulib_blake3_init(mulib_blake3_ctx *c)
{
  if(!c) return 0;
  memset(c,0,sizeof *c); memcpy(c->key,mulib_ch_iv32_,32); memcpy(c->cv,c->key,32);
  c->active=1; return 1;
}
int mulib_blake3_init_keyed(mulib_blake3_ctx *c,const unsigned char key[32])
{
  if(!c || !key) return 0;
  mulib_blake3_init(c);
  for(unsigned i=0;i<8;++i) c->key[i]=mulib_hc_le32(key+4*i);
  memcpy(c->cv,c->key,32); c->flags=16; return 1;
}
int mulib_blake3_update(mulib_blake3_ctx *c,const void *data,size_t n)
{
  const unsigned char *p=(const unsigned char *)data;
  if(!c || c->active!=1 || (!p && n) || n>UINT64_MAX-c->total) return 0;
  c->total+=n;
  while(n) {
    size_t k;
    if(c->used==64) {
      mulib_ch_b3output_ o=mulib_blake3_chunk_(c);
      if(c->blocks==15) {
        uint32_t cv[8]; uint64_t chunks=c->chunk+1;
        mulib_blake3_cv_(&o,cv,c->intrinsics);
        while((chunks&1)==0) {
          o=mulib_blake3_parent_(c,c->stack[--c->stack_length],cv);
          mulib_blake3_cv_(&o,cv,c->intrinsics); chunks>>=1;
        }
        memcpy(c->stack[c->stack_length++],cv,32);
        ++c->chunk; c->blocks=0; memcpy(c->cv,c->key,32);
        mulib_hc_wipe(cv,sizeof cv);
      } else {
        o.flags&=~2u; mulib_blake3_cv_(&o,c->cv,c->intrinsics); ++c->blocks;
      }
      c->used=0; memset(c->block,0,64); mulib_hc_wipe(&o,sizeof o);
    }
    k=64-c->used; if(k>n) k=n;
    memcpy(c->block+c->used,p,k); c->used+=(unsigned)k; p+=k; n-=k;
  }
  return 1;
}
int mulib_blake3_final_seek(const mulib_blake3_ctx *c,uint64_t seek,void *out,size_t n)
{
  unsigned char *p=(unsigned char *)out;
  mulib_ch_b3output_ o; uint32_t cv[8],words[16]; unsigned char bytes[64];
  uint64_t block=seek/64; unsigned offset=(unsigned)(seek%64);
  if(!c || c->active!=1 || (!p && n) || n>UINT64_MAX-seek) return 0;
  o=mulib_blake3_chunk_(c);
  for(unsigned i=c->stack_length;i>0;--i) {
    mulib_blake3_cv_(&o,cv,c->intrinsics); o=mulib_blake3_parent_(c,c->stack[i-1],cv);
  }
  while(n) {
    size_t k=64-offset; if(k>n) k=n;
    mulib_blake3_compress_(&o,block++,o.flags|8,words,c->intrinsics);
    for(unsigned i=0;i<16;++i) mulib_hc_putle(bytes+4*i,words[i],4);
    memcpy(p,bytes+offset,k); p+=k; n-=k; offset=0;
  }
  mulib_hc_wipe(&o,sizeof o); mulib_hc_wipe(cv,sizeof cv);
  mulib_hc_wipe(words,sizeof words); mulib_hc_wipe(bytes,sizeof bytes); return 1;
}
int mulib_blake3_final(const mulib_blake3_ctx *c,void *out,size_t n)
{ return mulib_blake3_final_seek(c,0,out,n); }
int mulib_blake3_init_derive_key(mulib_blake3_ctx *c,const void *context,size_t n)
{
  mulib_blake3_ctx temp; unsigned char key[32]; int ok;
  if(!c || (!context && n)) return 0;
  mulib_blake3_init(&temp); temp.flags=32;
  ok=mulib_blake3_update(&temp,context,n) && mulib_blake3_final(&temp,key,32);
  if(ok) { mulib_blake3_init_keyed(c,key); c->flags=64; }
  mulib_hc_wipe(&temp,sizeof temp); mulib_hc_wipe(key,sizeof key); return ok;
}
int mulib_blake3_init_intrinsics(mulib_blake3_ctx *c)
{ if(!mulib_blake3_init(c)) return 0; c->intrinsics=MULIB_HASH_SSE2; return 1; }
int mulib_blake3_init_keyed_intrinsics(mulib_blake3_ctx *c,const unsigned char key[32])
{ if(!mulib_blake3_init_keyed(c,key)) return 0; c->intrinsics=MULIB_HASH_SSE2; return 1; }
int mulib_blake3_init_derive_key_intrinsics(mulib_blake3_ctx *c,const void *p,size_t n)
{ if(!mulib_blake3_init_derive_key(c,p,n)) return 0; c->intrinsics=MULIB_HASH_SSE2; return 1; }
static int mulib_blake3_once_(const void *p,size_t n,void *out,size_t on,unsigned simd)
{
  mulib_blake3_ctx c; int ok;
  mulib_blake3_init(&c); c.intrinsics=simd;
  ok=mulib_blake3_update(&c,p,n) && mulib_blake3_final(&c,out,on);
  mulib_hc_wipe(&c,sizeof c); return ok;
}
int mulib_blake3(const void *p,size_t n,void *out,size_t on)
{ return mulib_blake3_once_(p,n,out,on,0); }
int mulib_blake3_intrinsics(const void *p,size_t n,void *out,size_t on)
{ return mulib_blake3_once_(p,n,out,on,1); }
int mulib_blake3_keyed(const unsigned char key[32],const void *p,size_t n,void *out,size_t on)
{
  mulib_blake3_ctx c; int ok;
  if(!mulib_blake3_init_keyed(&c,key)) return 0;
  ok=mulib_blake3_update(&c,p,n) && mulib_blake3_final(&c,out,on);
  mulib_hc_wipe(&c,sizeof c); return ok;
}
int mulib_blake3_derive_key(const void *context,size_t cn,const void *p,size_t n,void *out,size_t on)
{
  mulib_blake3_ctx c; int ok;
  if(!mulib_blake3_init_derive_key(&c,context,cn)) return 0;
  ok=mulib_blake3_update(&c,p,n) && mulib_blake3_final(&c,out,on);
  mulib_hc_wipe(&c,sizeof c); return ok;
}
static void mulib_ch_sipround_(uint64_t v[4])
{
  v[0]+=v[1]; v[1]=mulib_hc_r64(v[1],51)^v[0]; v[0]=mulib_hc_r64(v[0],32);
  v[2]+=v[3]; v[3]=mulib_hc_r64(v[3],48)^v[2];
  v[0]+=v[3]; v[3]=mulib_hc_r64(v[3],43)^v[0];
  v[2]+=v[1]; v[1]=mulib_hc_r64(v[1],47)^v[2]; v[2]=mulib_hc_r64(v[2],32);
}
static int mulib_ch_sip_(const unsigned char key[16],const void *data,size_t n,unsigned char out[8],unsigned rounds,unsigned final)
{
  const unsigned char *p=(const unsigned char *)data;
  uint64_t v[4],k0,k1,tail=(uint64_t)(n&255)<<56;
  if(!key || !out || (!p && n)) return 0;
  k0=mulib_hc_le64(key); k1=mulib_hc_le64(key+8);
  v[0]=UINT64_C(0x736f6d6570736575)^k0; v[1]=UINT64_C(0x646f72616e646f6d)^k1;
  v[2]=UINT64_C(0x6c7967656e657261)^k0; v[3]=UINT64_C(0x7465646279746573)^k1;
  while(n>=8) {
    uint64_t m=mulib_hc_le64(p); v[3]^=m;
    for(unsigned i=0;i<rounds;++i) mulib_ch_sipround_(v);
    v[0]^=m; n-=8; p+=8;
  }
  for(size_t i=0;i<n;++i) tail|=(uint64_t)p[i]<<(8*i);
  v[3]^=tail; for(unsigned i=0;i<rounds;++i) mulib_ch_sipround_(v);
  v[0]^=tail; v[2]^=255; for(unsigned i=0;i<final;++i) mulib_ch_sipround_(v);
  mulib_hc_putle(out,v[0]^v[1]^v[2]^v[3],8);
  mulib_hc_wipe(v,sizeof v); mulib_hc_wipe(&k0,sizeof k0); mulib_hc_wipe(&k1,sizeof k1); return 1;
}
int mulib_siphash24(const unsigned char key[16],const void *p,size_t n,unsigned char out[8])
{ return mulib_ch_sip_(key,p,n,out,2,4); }
int mulib_siphash13(const unsigned char key[16],const void *p,size_t n,unsigned char out[8])
{ return mulib_ch_sip_(key,p,n,out,1,3); }
/* SP 800-185 encoding helpers; lengths limited to UINT64_MAX bits. */
static void mulib_ch_encode_(mulib_sha_ctx *c,uint64_t x,unsigned right)
{
  unsigned char bytes[9]; unsigned n=1; uint64_t t=x;
  while((t>>=8)!=0) ++n;
  if(right) { mulib_hc_putbe(bytes,x,n); bytes[n]=(unsigned char)n; }
  else { bytes[0]=(unsigned char)n; mulib_hc_putbe(bytes+1,x,n); }
  mulib_sha_update(c,bytes,n+1);
}
static void mulib_ch_string_(mulib_sha_ctx *c,const void *p,size_t n)
{ mulib_ch_encode_(c,(uint64_t)n*8,0); mulib_sha_update(c,p,n); }
static void mulib_ch_pad_(mulib_sha_ctx *c)
{
  static const unsigned char zero[168]={0};
  if(c->used) mulib_sha_update(c,zero,c->rate-c->used);
}
static int mulib_ch_kmac_(const void *key,size_t kn,const void *data,size_t n,const void *custom,size_t cn,void *out,size_t on,unsigned bits,unsigned xof,unsigned simd)
{
  mulib_sha_ctx c; int ok;
  if((!key && kn) || (!data && n) || (!custom && cn) || (!out && on)
     || kn>UINT64_MAX/8 || cn>UINT64_MAX/8 || on>UINT64_MAX/8) return 0;
  mulib_sha_init(&c,bits==128 ? MULIB_SHAKE128 : MULIB_SHAKE256); c.domain=4; c.intrinsics=simd;
  mulib_ch_encode_(&c,c.rate,0); mulib_ch_string_(&c,"KMAC",4);
  mulib_ch_string_(&c,custom,cn); mulib_ch_pad_(&c);
  mulib_ch_encode_(&c,c.rate,0); mulib_ch_string_(&c,key,kn); mulib_ch_pad_(&c);
  ok=mulib_sha_update(&c,data,n);
  mulib_ch_encode_(&c,xof ? 0 : (uint64_t)on*8,1);
  if(ok) ok=mulib_sha_squeeze(&c,out,on);
  mulib_hc_wipe(&c,sizeof c); return ok;
}
#define MULIB_CH_KMAC_(name,bits,xof,simd) \
int name(const void *key,size_t kn,const void *p,size_t n,const void *custom,size_t cn,void *out,size_t on) \
{ return mulib_ch_kmac_(key,kn,p,n,custom,cn,out,on,bits,xof,simd); }
MULIB_CH_KMAC_(mulib_kmac128,128,0,0)
MULIB_CH_KMAC_(mulib_kmac128_intrinsics,128,0,1)
MULIB_CH_KMAC_(mulib_kmac256,256,0,0)
MULIB_CH_KMAC_(mulib_kmac256_intrinsics,256,0,1)
MULIB_CH_KMAC_(mulib_kmacxof128,128,1,0)
MULIB_CH_KMAC_(mulib_kmacxof128_intrinsics,128,1,1)
MULIB_CH_KMAC_(mulib_kmacxof256,256,1,0)
MULIB_CH_KMAC_(mulib_kmacxof256_intrinsics,256,1,1)
#undef MULIB_CH_KMAC_
static unsigned char mulib_ch_xtime_(unsigned char a)
{ return (unsigned char)((a<<1)^((0u-(a>>7))&0x1b)); }
static unsigned char mulib_ch_gfmul_(unsigned char a,unsigned char b)
{
  unsigned char r=0;
  for(unsigned i=0;i<8;++i) { r^=(unsigned char)(a&(0u-(b&1))); a=mulib_ch_xtime_(a); b>>=1; }
  return r;
}
static unsigned char mulib_ch_sbox_(unsigned char x)
{
  unsigned char y=1,base=x;
  /* Inversion x^254, also maps zero to zero. Exponent is public and fixed. */
  for(unsigned e=254;e;e>>=1) { if(e&1) y=mulib_ch_gfmul_(y,base); base=mulib_ch_gfmul_(base,base); }
  return (unsigned char)(y^((y<<1)|(y>>7))^((y<<2)|(y>>6))
                          ^((y<<3)|(y>>5))^((y<<4)|(y>>4))^0x63);
}
static unsigned mulib_ch_aes_keys_(const unsigned char *key,size_t kn,unsigned char keys[240])
{
  unsigned rounds=(unsigned)(kn/4)+6; size_t used=kn,total=16*(rounds+1);
  unsigned char rcon=1,tmp[4]; memcpy(keys,key,kn);
  while(used<total) {
    memcpy(tmp,keys+used-4,4);
    if(used%kn==0) {
      unsigned char first=tmp[0];
      tmp[0]=mulib_ch_sbox_(tmp[1])^rcon; tmp[1]=mulib_ch_sbox_(tmp[2]);
      tmp[2]=mulib_ch_sbox_(tmp[3]); tmp[3]=mulib_ch_sbox_(first); rcon=mulib_ch_xtime_(rcon);
    } else if(kn==32 && used%kn==16) for(unsigned i=0;i<4;++i) tmp[i]=mulib_ch_sbox_(tmp[i]);
    for(unsigned i=0;i<4;++i) { keys[used]=keys[used-kn]^tmp[i]; ++used; }
  }
  mulib_hc_wipe(tmp,sizeof tmp); return rounds;
}
static void mulib_ch_aes_(const unsigned char keys[240],unsigned rounds,const unsigned char in[16],unsigned char out[16],unsigned simd)
{
#if defined(__AES__) && !defined(MULIB_HASH_NO_INTRINSICS)
  if(simd) {
    __m128i x=_mm_loadu_si128((const __m128i *)(const void *)in);
    x=_mm_xor_si128(x,_mm_loadu_si128((const __m128i *)(const void *)keys));
    for(unsigned r=1;r<rounds;++r) x=_mm_aesenc_si128(x,_mm_loadu_si128((const __m128i *)(const void *)(keys+16*r)));
    x=_mm_aesenclast_si128(x,_mm_loadu_si128((const __m128i *)(const void *)(keys+16*rounds)));
    _mm_storeu_si128((__m128i *)(void *)out,x); return;
  }
#else
  (void)simd;
#endif
  unsigned char s[16],t[16];
  for(unsigned i=0;i<16;++i) s[i]=in[i]^keys[i];
  for(unsigned r=1;r<=rounds;++r) {
    for(unsigned col=0;col<4;++col) for(unsigned row=0;row<4;++row)
      t[4*col+row]=mulib_ch_sbox_(s[4*((col+row)%4)+row]);
    if(r!=rounds) for(unsigned col=0;col<4;++col) {
      unsigned i=4*col; unsigned char a=t[i],b=t[i+1],c=t[i+2],d=t[i+3],x=a^b^c^d;
      t[i]^=x^mulib_ch_xtime_(a^b); t[i+1]^=x^mulib_ch_xtime_(b^c);
      t[i+2]^=x^mulib_ch_xtime_(c^d); t[i+3]^=x^mulib_ch_xtime_(d^a);
    }
    for(unsigned i=0;i<16;++i) s[i]=t[i]^keys[16*r+i];
  }
  memcpy(out,s,16); mulib_hc_wipe(s,sizeof s); mulib_hc_wipe(t,sizeof t);
}
static int mulib_ch_pmac_(const void *key,size_t kn,const void *data,size_t n,unsigned char tag[16],unsigned simd)
{
  const unsigned char *p=(const unsigned char *)data;
  unsigned char keys[240],l[sizeof(size_t)*8][16],inverse[16],offset[16]={0},sum[16]={0},tmp[16]={0};
  unsigned rounds,carry; size_t index=1;
  if(!key || (kn!=16 && kn!=24 && kn!=32) || (!p && n) || !tag) return 0;
  rounds=mulib_ch_aes_keys_((const unsigned char *)key,kn,keys);
  mulib_ch_aes_(keys,rounds,tmp,l[0],simd);
  for(unsigned j=1;j<sizeof(size_t)*8;++j) {
    carry=l[j-1][0]>>7;
    for(unsigned i=0;i<16;++i) l[j][i]=(unsigned char)((l[j-1][i]<<1)|(i<15 ? l[j-1][i+1]>>7 : 0));
    l[j][15]^=(unsigned char)((0u-carry)&0x87);
  }
  carry=l[0][15]&1;
  for(unsigned i=0;i<16;++i) inverse[i]=(unsigned char)((l[0][i]>>1)|(i ? (l[0][i-1]&1)<<7 : 0));
  inverse[0]^=(unsigned char)((0u-carry)&0x80); inverse[15]^=(unsigned char)((0u-carry)&0x43);
  while(n>16) {
    size_t x=index++; unsigned z=0;
    while((x&1)==0) { ++z; x>>=1; }
    for(unsigned i=0;i<16;++i) { offset[i]^=l[z][i]; tmp[i]=p[i]^offset[i]; }
    mulib_ch_aes_(keys,rounds,tmp,tmp,simd);
    for(unsigned i=0;i<16;++i) sum[i]^=tmp[i];
    p+=16; n-=16;
  }
  for(size_t i=0;i<n;++i) sum[i]^=p[i];
  if(n==16) for(unsigned i=0;i<16;++i) sum[i]^=inverse[i];
  else sum[n]^=0x80;
  mulib_ch_aes_(keys,rounds,sum,tag,simd);
  mulib_hc_wipe(keys,sizeof keys); mulib_hc_wipe(l,sizeof l); mulib_hc_wipe(inverse,sizeof inverse);
  mulib_hc_wipe(offset,sizeof offset); mulib_hc_wipe(sum,sizeof sum); mulib_hc_wipe(tmp,sizeof tmp); return 1;
}
int mulib_pmac(const void *key,size_t kn,const void *p,size_t n,unsigned char tag[16])
{ return mulib_ch_pmac_(key,kn,p,n,tag,0); }
int mulib_pmac_intrinsics(const void *key,size_t kn,const void *p,size_t n,unsigned char tag[16])
{ return mulib_ch_pmac_(key,kn,p,n,tag,1); }
#ifdef __cplusplus
}
#endif
#endif
