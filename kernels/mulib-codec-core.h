/** @file mulib-codec-core.h
 * @brief Checked storage, bit I/O and wire integers shared by compression codecs.
 * Define MULIB_IMPL_CODEC_CORE and MULIB_IMPL_BUFFER in one translation unit.
 * No external libraries, global mutable state, or filesystem operations.
 */
#ifndef MULIB_CODEC_CORE_H
#define MULIB_CODEC_CORE_H
#include "mulib-buffer.h"
#ifdef __cplusplus
extern "C" {
#endif
/** @brief Common codec results. Every public codec append is failure-atomic. */
enum mulib_codec_result {
  MULIB_CODEC_OK = 0, MULIB_CODEC_EINVAL = -1, MULIB_CODEC_EFORMAT = -2,
  MULIB_CODEC_ELIMIT = -3, MULIB_CODEC_ENOMEM = -4,
  MULIB_CODEC_EUNSUPPORTED = -5, MULIB_CODEC_ECHECKSUM = -6
};
/** @brief Validate an initialized byte buffer without modifying it. */
static inline int mulib_codec_buffer_valid (const mulib_buffer_t *b)
{ return b && b->length <= b->capacity && (b->data || !b->capacity); }
/** @brief Reserve at most limit bytes; never allocate an advertised dictionary. */
int mulib_codec_reserve (mulib_buffer_t *b, size_t n, size_t limit);
/** @brief Append nonaliasing bytes within a hard total-length bound. */
int mulib_codec_append (mulib_buffer_t *b, const void *p, size_t n, size_t limit);
/** @brief Expand a checked overlapping LZ back-reference. Distance is positive. */
int mulib_codec_match (mulib_buffer_t *b, size_t distance, size_t n, size_t limit);
/** @brief CRC-64/XZ, seed zero, including the final complement. */
uint64_t mulib_codec_crc64 (const void *p, size_t n);
/** @brief XXH64 with seed zero, for Zstandard content checksums. */
uint64_t mulib_codec_xxh64 (const void *p, size_t n);
/** @brief Endian-independent wire loads/stores; caller supplies accessible bytes. */
static inline uint32_t mulib_codec_le32 (const unsigned char *p)
{ return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24; }
static inline uint64_t mulib_codec_le64 (const unsigned char *p)
{ return mulib_codec_le32 (p) | (uint64_t)mulib_codec_le32 (p+4)<<32; }
static inline uint32_t mulib_codec_be32 (const unsigned char *p)
{ return (uint32_t)p[0]<<24 | (uint32_t)p[1]<<16 | (uint32_t)p[2]<<8 | p[3]; }
static inline void mulib_codec_put32 (unsigned char *p, uint32_t v)
{ unsigned i; for (i=0;i<4;++i) p[i]=(unsigned char)(v>>(8*i)); }
static inline void mulib_codec_put64 (unsigned char *p, uint64_t v)
{ mulib_codec_put32 (p,(uint32_t)v); mulib_codec_put32 (p+4,(uint32_t)(v>>32)); }
static inline void mulib_codec_putbe32 (unsigned char *p, uint32_t v)
{ unsigned i; for (i=0;i<4;++i) p[i]=(unsigned char)(v>>(24-8*i)); }
/** @brief MSB-first bounded reader; zero initialize pos/bits/count. */
typedef struct mulib_codec_bits {
  const unsigned char *data;
  size_t size, pos;
  unsigned bits, count;
} mulib_codec_bits_t;
/** @brief Read 0..32 bits; returns false on truncation. */
static inline int mulib_codec_get (mulib_codec_bits_t *b, unsigned n, uint32_t *v)
{
  uint32_t x=0;
  if (n>32) return 0;
  while (n--) {
    if (!b->count) { if (b->pos==b->size) return 0; b->bits=b->data[b->pos++]; b->count=8; }
    x=(x<<1)|((b->bits>>--b->count)&1u);
  }
  *v=x; return 1;
}
/** @brief MSB-first writer; owns bytes and requires a final flush. */
typedef struct mulib_codec_writer { mulib_buffer_t bytes; unsigned bits, count; } mulib_codec_writer_t;
/** @brief Write 0..32 bits, returning a codec result. */
int mulib_codec_put (mulib_codec_writer_t *w, uint32_t v, unsigned n);
/** @brief Pad the last byte with zero bits. */
int mulib_codec_flush (mulib_codec_writer_t *w);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CODEC_CORE) && !defined(MULIB_CODEC_CORE_IMPLEMENTED)
#define MULIB_CODEC_CORE_IMPLEMENTED
int mulib_codec_reserve (mulib_buffer_t *b, size_t n, size_t limit)
{
  size_t cap; void *p;
  if (!mulib_codec_buffer_valid (b)) return MULIB_CODEC_EINVAL;
  if (n>limit) return MULIB_CODEC_ELIMIT;
  if (n<=b->capacity) return 0;
  cap=b->capacity ? b->capacity : (limit<64 ? limit : 64);
  while (cap<n) { if (cap>limit/2) { cap=limit; break; } cap*=2; }
  p=realloc (b->data,cap);
  if (!p) return MULIB_CODEC_ENOMEM;
  b->data=(unsigned char *)p; b->capacity=cap; return 0;
}
int mulib_codec_append (mulib_buffer_t *b, const void *p, size_t n, size_t limit)
{
  int rc;
  if (!mulib_codec_buffer_valid (b) || (!p && n)) return MULIB_CODEC_EINVAL;
  if (b->length>limit || n>limit-b->length) return MULIB_CODEC_ELIMIT;
  rc=mulib_codec_reserve (b,b->length+n,limit); if (rc) return rc;
  if (n) memcpy (b->data+b->length,p,n);
  b->length+=n; return 0;
}
int mulib_codec_match (mulib_buffer_t *b, size_t distance, size_t n, size_t limit)
{
  size_t i; int rc;
  if (!mulib_codec_buffer_valid (b)) return MULIB_CODEC_EINVAL;
  if (!distance || distance>b->length) return MULIB_CODEC_EFORMAT;
  if (b->length>limit || n>limit-b->length) return MULIB_CODEC_ELIMIT;
  rc=mulib_codec_reserve (b,b->length+n,limit); if (rc) return rc;
  for (i=0;i<n;++i) { b->data[b->length]=b->data[b->length-distance]; ++b->length; }
  return 0;
}
uint64_t mulib_codec_crc64 (const void *data, size_t n)
{
  const unsigned char *p=(const unsigned char *)data;
  uint64_t c=UINT64_MAX; size_t i; unsigned k;
  for (i=0;i<n;++i) { c^=p[i]; for (k=0;k<8;++k) c=(c>>1)^((c&1)?UINT64_C(0xc96c5795d7870f42):0); }
  return ~c;
}
static uint64_t mulib_codec_rot64_ (uint64_t v, unsigned n)
{ return (v<<n)|(v>>(64-n)); }
static uint64_t mulib_codec_xxh_round_ (uint64_t a, uint64_t v)
{ return mulib_codec_rot64_ (a+v*UINT64_C(14029467366897019727),31)*UINT64_C(11400714785074694791); }
uint64_t mulib_codec_xxh64 (const void *data, size_t n)
{
  const unsigned char *p=(const unsigned char *)data; size_t at=0;
  const uint64_t p1=UINT64_C(11400714785074694791),p2=UINT64_C(14029467366897019727),p3=UINT64_C(1609587929392839161),p4=UINT64_C(9650029242287828579),p5=UINT64_C(2870177450012600261);
  uint64_t h;
  if (n>=32) {
    uint64_t v[4]={p1+p2,p2,0,UINT64_C(0)-p1}; unsigned i;
    do { for (i=0;i<4;++i) v[i]=mulib_codec_xxh_round_ (v[i],mulib_codec_le64 (p+at+8*i)); at+=32; } while (n-at>=32);
    h=mulib_codec_rot64_ (v[0],1)+mulib_codec_rot64_ (v[1],7)+mulib_codec_rot64_ (v[2],12)+mulib_codec_rot64_ (v[3],18);
    for (i=0;i<4;++i) h=(h^mulib_codec_xxh_round_ (0,v[i]))*p1+p4;
  } else h=p5;
  h+=n;
  while (n-at>=8) { h^=mulib_codec_xxh_round_ (0,mulib_codec_le64 (p+at)); h=mulib_codec_rot64_ (h,27)*p1+p4; at+=8; }
  if (n-at>=4) { h^=(uint64_t)mulib_codec_le32 (p+at)*p1; h=mulib_codec_rot64_ (h,23)*p2+p3; at+=4; }
  while (at<n) { h^=(uint64_t)p[at++]*p5; h=mulib_codec_rot64_ (h,11)*p1; }
  h^=h>>33; h*=p2; h^=h>>29; h*=p3; return h^(h>>32);
}
int mulib_codec_put (mulib_codec_writer_t *w, uint32_t v, unsigned n)
{
  if (!w || n>32) return MULIB_CODEC_EINVAL;
  while (n--) {
    w->bits=(w->bits<<1)|((v>>n)&1u);
    if (++w->count==8) {
      unsigned char c=(unsigned char)w->bits;
      if (!mulib_buffer_append (&w->bytes,&c,1)) return MULIB_CODEC_ENOMEM;
      w->bits=w->count=0;
    }
  }
  return 0;
}
int mulib_codec_flush (mulib_codec_writer_t *w)
{ return w->count ? mulib_codec_put (w,0,8-w->count) : 0; }
#endif
