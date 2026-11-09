/** @file mulib-lzma.h
 * @brief Dependency-free LZMA1 and LZMA2 range/LZ codecs and LZMA-alone files.
 * Define MULIB_IMPL_LZMA, CODEC_CORE and BUFFER. No dictionary-sized allocation.
 */
#ifndef MULIB_LZMA_H
#define MULIB_LZMA_H
#include "mulib-codec-core.h"
#ifdef __cplusplus
extern "C" {
#endif
/** @brief Append an LZMA-alone stream (lc=3, lp=0, pb=2, 8 MiB dictionary, EOS).
 * Greedy bounded LZ matching and adaptive range coding. Input may alias out. */
int mulib_lzma_encode (const void *data, size_t size, mulib_buffer_t *out);
/** @brief Decode exactly one LZMA-alone stream, known or unknown output size.
 * All legal LZMA1 properties are supported; limit bounds decoded bytes. */
int mulib_lzma_decode (const void *data, size_t size, size_t limit, mulib_buffer_t *out);
/** @brief Encode raw LZMA-302 with EOS, the shared payload for lzip. */
int mulib_lzma_raw_encode (const void *data, size_t size, mulib_buffer_t *out);
/** @brief Decode raw LZMA1 through EOS using the packed lc/lp/pb property byte.
 * consumed and output change only on success; trailing bytes are not consumed. */
int mulib_lzma_raw_decode (const void *data, size_t size, unsigned properties,
                         uint32_t dictionary, size_t limit, size_t *consumed,
                         mulib_buffer_t *out);
/** @brief Encode an LZMA2 stream; independent compressed or stored 32 KiB chunks. */
int mulib_lzma2_encode (const void *data, size_t size, mulib_buffer_t *out);
/** @brief Decode LZMA2 including dictionary/model resets and stored chunks.
 * Stops at its end control byte, reports consumed, and appends atomically. */
int mulib_lzma2_decode (const void *data, size_t size, uint32_t dictionary,
                      size_t limit, size_t *consumed, mulib_buffer_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_LZMA) && !defined(MULIB_LZMA_IMPLEMENTED)
#define MULIB_LZMA_IMPLEMENTED
enum { MULIB_LZMA_LITERAL_=1846 };
typedef struct mulib_lzma_model_ {
  uint16_t *p;
  size_t count, start;
  uint32_t rep[4], dictionary;
  unsigned lc, lp, pb, state;
} mulib_lzma_model_;
typedef struct mulib_lzma_range_ {
  const unsigned char *data;
  size_t size, at;
  uint32_t range, code;
  int error;
} mulib_lzma_range_;
static int mulib_lzma_properties_ (mulib_lzma_model_ *m, unsigned prop)
{
  size_t count; uint16_t *p;
  if (prop>=225) return MULIB_CODEC_EFORMAT;
  m->lc=prop%9; prop/=9; m->lp=prop%5; m->pb=prop/5;
  count=MULIB_LZMA_LITERAL_+((size_t)768<<(m->lc+m->lp));
  p=(uint16_t *)realloc (m->p,count*sizeof (*p)); if (!p) return MULIB_CODEC_ENOMEM;
  m->p=p; m->count=count; return 0;
}
static void mulib_lzma_reset_ (mulib_lzma_model_ *m)
{
  size_t i; for (i=0;i<m->count;++i) m->p[i]=1024;
  m->state=0; for (i=0;i<4;++i) m->rep[i]=0;
}
static void mulib_lzma_normalize_ (mulib_lzma_range_ *r)
{
  if (r->range<UINT32_C(0x1000000)) {
    if (r->at==r->size) { r->error=1; return; }
    r->range<<=8; r->code=(r->code<<8)|r->data[r->at++];
  }
}
static unsigned mulib_lzma_bit_ (mulib_lzma_range_ *r, uint16_t *p)
{
  uint32_t bound=(r->range>>11)*(*p); unsigned bit;
  if (r->code<bound) { bit=0; r->range=bound; *p=(uint16_t)(*p+((2048-*p)>>5)); }
  else { bit=1; r->code-=bound; r->range-=bound; *p=(uint16_t)(*p-(*p>>5)); }
  mulib_lzma_normalize_ (r); return bit;
}
static uint32_t mulib_lzma_tree_ (mulib_lzma_range_ *r, uint16_t *p, unsigned n, int reverse)
{
  uint32_t symbol=1,value=0; unsigned i,bit;
  for (i=0;i<n;++i) { bit=mulib_lzma_bit_ (r,p+symbol); symbol=(symbol<<1)|bit; value|=(uint32_t)bit<<i; }
  return reverse?value:symbol-((uint32_t)1<<n);
}
static unsigned mulib_lzma_length_ (mulib_lzma_range_ *r, uint16_t *p, unsigned ps)
{
  if (!mulib_lzma_bit_ (r,p)) return 2+mulib_lzma_tree_ (r,p+2+ps*8,3,0);
  if (!mulib_lzma_bit_ (r,p+1)) return 10+mulib_lzma_tree_ (r,p+130+ps*8,3,0);
  return 18+mulib_lzma_tree_ (r,p+258,8,0);
}
/* expected==SIZE_MAX means EOS required. Chunk mode forbids EOS. */
static int mulib_lzma_decode_part_ (mulib_lzma_model_ *m, const unsigned char *p,
                                  size_t n, size_t expected, int chunk,
                                  size_t limit, size_t *used, mulib_buffer_t *out)
{
  mulib_lzma_range_ r; size_t before=out->length; unsigned i;
  if (n<5 || p[0]) return MULIB_CODEC_EFORMAT;
  r.data=p; r.size=n; r.at=5; r.range=UINT32_MAX; r.code=mulib_codec_be32 (p+1); r.error=0;
  if (r.code==UINT32_MAX) return MULIB_CODEC_EFORMAT;
  for (;;) {
    size_t pos=out->length-m->start; unsigned ps=(unsigned)pos&((1u<<m->pb)-1);
    unsigned state=m->state,len; uint32_t distance;
    if (expected!=SIZE_MAX && out->length-before==expected && !r.code) break;
    if (!mulib_lzma_bit_ (&r,m->p+state*16+ps)) {
      unsigned prev=pos?out->data[out->length-1]:0, symbol=1;
      size_t context=((pos&(((size_t)1<<m->lp)-1))<<m->lc)+(prev>>(8-m->lc));
      uint16_t *lit=m->p+MULIB_LZMA_LITERAL_+context*768;
      if (state>=7) {
        unsigned match;
        if ((size_t)m->rep[0]>=pos || m->rep[0]>=m->dictionary) return MULIB_CODEC_EFORMAT;
        match=out->data[out->length-m->rep[0]-1];
        while (symbol<256) {
          unsigned mb=(match>>7)&1, bit; match<<=1;
          bit=mulib_lzma_bit_ (&r,lit+((1+mb)<<8)+symbol); symbol=(symbol<<1)|bit;
          if (bit!=mb) break;
        }
      }
      while (symbol<256) symbol=(symbol<<1)|mulib_lzma_bit_ (&r,lit+symbol);
      if (r.error || (expected!=SIZE_MAX && out->length-before>=expected)) return MULIB_CODEC_EFORMAT;
      { unsigned char c=(unsigned char)symbol; int rc=mulib_codec_append (out,&c,1,limit); if (rc) return rc; }
      m->state=state<4?0:state<10?state-3:state-6;
      continue;
    }
    if (mulib_lzma_bit_ (&r,m->p+192+state)) {
      if (!mulib_lzma_bit_ (&r,m->p+204+state)) {
        if (!mulib_lzma_bit_ (&r,m->p+240+state*16+ps)) {
          len=1; m->state=state<7?9:11; goto copy;
        }
      } else {
        unsigned index;
        if (!mulib_lzma_bit_ (&r,m->p+216+state)) index=1;
        else index=mulib_lzma_bit_ (&r,m->p+228+state)?3:2;
        distance=m->rep[index]; for (i=index;i>0;--i) m->rep[i]=m->rep[i-1]; m->rep[0]=distance;
      }
      len=mulib_lzma_length_ (&r,m->p+1332,ps); m->state=state<7?8:11;
    } else {
      unsigned slot,extra;
      len=mulib_lzma_length_ (&r,m->p+818,ps); m->state=state<7?7:10;
      slot=mulib_lzma_tree_ (&r,m->p+432+(len-2<4?len-2:3)*64,6,0);
      distance=slot;
      if (slot>=4) {
        extra=(slot>>1)-1; distance=(uint32_t)(2|(slot&1))<<extra;
        if (slot<14) {
          unsigned node=1,k; uint32_t value=0;
          for (k=0;k<extra;++k) {
            unsigned bit=mulib_lzma_bit_ (&r,m->p+688+distance-slot-1+node);
            node=(node<<1)|bit; value|=(uint32_t)bit<<k;
          }
          distance+=value;
        } else {
          uint32_t value=0; unsigned k;
          for (k=0;k<extra-4;++k) {
            unsigned bit; r.range>>=1; bit=r.code>=r.range;
            if (bit) r.code-=r.range;
            value=(value<<1)|bit; mulib_lzma_normalize_ (&r);
          }
          distance+=(value<<4)+mulib_lzma_tree_ (&r,m->p+802,4,1);
        }
      }
      if (distance==UINT32_MAX) {
        if (chunk || len!=2 || r.error || r.code
            || (expected!=SIZE_MAX && out->length-before!=expected)) return MULIB_CODEC_EFORMAT;
        break;
      }
      for (i=3;i>0;--i) m->rep[i]=m->rep[i-1];
      m->rep[0]=distance;
    }
copy:
    if (r.error || (size_t)m->rep[0]>=pos || m->rep[0]>=m->dictionary) return MULIB_CODEC_EFORMAT;
    if (expected!=SIZE_MAX && len>expected-(out->length-before)) return MULIB_CODEC_EFORMAT;
    { int rc=mulib_codec_match (out,(size_t)m->rep[0]+1,len,limit); if (rc) return rc; }
  }
  if (r.error) return MULIB_CODEC_EFORMAT;
  *used=r.at; return 0;
}
typedef struct mulib_lzma_encoder_ {
  mulib_buffer_t bytes; uint64_t low; uint32_t range;
  size_t pending; unsigned cache; int error;
} mulib_lzma_encoder_;
static void mulib_lzma_shift_ (mulib_lzma_encoder_ *r)
{
  uint32_t low=(uint32_t)r->low; unsigned carry=(unsigned)(r->low>>32);
  if (low<UINT32_C(0xff000000) || carry) {
    unsigned temp=r->cache;
    do { unsigned char c=(unsigned char)(temp+carry);
      if (!mulib_buffer_append (&r->bytes,&c,1)) { r->error=1; break; }
      temp=255;
    } while (--r->pending);
    r->cache=low>>24;
  }
  ++r->pending; r->low=(uint32_t)(low<<8);
}
static void mulib_lzma_write_ (mulib_lzma_encoder_ *r, uint16_t *p, unsigned bit)
{
  uint32_t bound=(r->range>>11)*(*p);
  if (!bit) { r->range=bound; *p=(uint16_t)(*p+((2048-*p)>>5)); }
  else { r->low+=bound; r->range-=bound; *p=(uint16_t)(*p-(*p>>5)); }
  if (r->range<UINT32_C(0x1000000)) { r->range<<=8; mulib_lzma_shift_ (r); }
}
static void mulib_lzma_write_tree_ (mulib_lzma_encoder_ *r, uint16_t *p,
                                   uint32_t value, unsigned n, int reverse)
{
  unsigned node=1,i,bit;
  for (i=0;i<n;++i) {
    bit=(value>>(reverse?i:n-i-1))&1u;
    mulib_lzma_write_ (r,p+node,bit); node=(node<<1)|bit;
  }
}
static void mulib_lzma_write_len_ (mulib_lzma_encoder_ *r, uint16_t *p, unsigned len, unsigned ps)
{
  if (len<10) { mulib_lzma_write_ (r,p,0); mulib_lzma_write_tree_ (r,p+2+ps*8,len-2,3,0); }
  else {
    mulib_lzma_write_ (r,p,1); mulib_lzma_write_ (r,p+1,len>=18);
    mulib_lzma_write_tree_ (r,p+(len<18?130+ps*8:258),len-(len<18?10:18),len<18?3:8,0);
  }
}
static void mulib_lzma_write_dist_ (mulib_lzma_encoder_ *r, mulib_lzma_model_ *m,
                                   uint32_t distance, unsigned len)
{
  unsigned slot=distance,extra,k;
  if (distance>=4) {
    unsigned log=0; uint32_t v=distance; while (v>>=1) ++log;
    slot=log*2+((distance>>(log-1))&1u);
  }
  mulib_lzma_write_tree_ (r,m->p+432+(len-2<4?len-2:3)*64,slot,6,0);
  if (slot<4) return;
  extra=(slot>>1)-1;
  if (slot<14) {
    uint32_t base=(uint32_t)(2|(slot&1))<<extra; unsigned node=1;
    for (k=0;k<extra;++k) {
      unsigned bit=(distance>>k)&1u;
      mulib_lzma_write_ (r,m->p+688+base-slot-1+node,bit); node=(node<<1)|bit;
    }
  } else {
    for (k=extra;k>4;--k) {
      r->range>>=1; if ((distance>>(k-1))&1u) r->low+=r->range;
      if (r->range<UINT32_C(0x1000000)) { r->range<<=8; mulib_lzma_shift_ (r); }
    }
    mulib_lzma_write_tree_ (r,m->p+802,distance&15,4,1);
  }
}
static int mulib_lzma_encode_part_ (const unsigned char *p, size_t n, int eos, mulib_buffer_t *out)
{
  mulib_lzma_model_ m; mulib_lzma_encoder_ r; size_t *last,at=0,i; int rc;
  memset (&m,0,sizeof (m)); memset (&r,0,sizeof (r)); r.range=UINT32_MAX; r.pending=1;
  rc=mulib_lzma_properties_ (&m,93); if (rc) return rc;
  mulib_lzma_reset_ (&m);
  last=(size_t *)malloc (16384*sizeof (*last)); if (!last) { free (m.p); return MULIB_CODEC_ENOMEM; }
  for (i=0;i<16384;++i) last[i]=SIZE_MAX;
  while (at<n && !r.error) {
    size_t len=0,prev=0; unsigned ps=(unsigned)at&3,state=m.state;
    if (n-at>=3) {
      unsigned hash=((unsigned)p[at]*251u+(unsigned)p[at+1]*31u+p[at+2])&16383u;
      prev=last[hash]; last[hash]=at;
      if (prev!=SIZE_MAX && at-prev<=8388608) while (len<273 && len<n-at && p[prev+len]==p[at+len]) ++len;
    }
    mulib_lzma_write_ (&r,m.p+state*16+ps,len>=3);
    if (len>=3) {
      uint32_t distance=(uint32_t)(at-prev-1);
      mulib_lzma_write_ (&r,m.p+192+state,0);
      mulib_lzma_write_len_ (&r,m.p+818,(unsigned)len,ps);
      mulib_lzma_write_dist_ (&r,&m,distance,(unsigned)len);
      m.rep[0]=distance; m.state=state<7?7:10; at+=len;
    } else {
      uint16_t *lit=m.p+MULIB_LZMA_LITERAL_+(at?p[at-1]>>5:0)*768;
      unsigned symbol=1,k,match=state>=7?p[at-m.rep[0]-1]:0,matched=state>=7;
      for (k=8;k>0;--k) {
        unsigned bit=(p[at]>>(k-1))&1u,mb=(match>>(k-1))&1u;
        mulib_lzma_write_ (&r,lit+symbol+(matched?((1+mb)<<8):0),bit);
        symbol=(symbol<<1)|bit; if (bit!=mb) matched=0;
      }
      m.state=state<4?0:state<10?state-3:state-6; ++at;
    }
  }
  if (eos && !r.error) {
    mulib_lzma_write_ (&r,m.p+m.state*16+((unsigned)at&3),1);
    mulib_lzma_write_ (&r,m.p+192+m.state,0);
    mulib_lzma_write_len_ (&r,m.p+818,2,(unsigned)at&3);
    mulib_lzma_write_dist_ (&r,&m,UINT32_MAX,2);
  }
  for (i=0;i<5 && !r.error;++i) mulib_lzma_shift_ (&r);
  rc=r.error?MULIB_CODEC_ENOMEM:mulib_codec_append (out,r.bytes.data,r.bytes.length,SIZE_MAX);
  free (last); free (m.p); mulib_buffer_free (&r.bytes); return rc;
}
int mulib_lzma_raw_encode (const void *data, size_t size, mulib_buffer_t *out)
{
  if ((!data && size) || !mulib_codec_buffer_valid (out)) return MULIB_CODEC_EINVAL;
  return mulib_lzma_encode_part_ ((const unsigned char *)data,size,1,out);
}
int mulib_lzma_encode (const void *data, size_t size, mulib_buffer_t *out)
{
  unsigned char h[13]={93,0,0,128,0,255,255,255,255,255,255,255,255};
  mulib_buffer_t tmp={0}; int rc;
  if ((!data && size) || !mulib_codec_buffer_valid (out)) return MULIB_CODEC_EINVAL;
  rc=mulib_codec_append (&tmp,h,13,SIZE_MAX);
  if (!rc) rc=mulib_lzma_encode_part_ ((const unsigned char *)data,size,1,&tmp);
  if (!rc && !mulib_buffer_append (out,tmp.data,tmp.length)) rc=MULIB_CODEC_ENOMEM;
  mulib_buffer_free (&tmp); return rc;
}
int mulib_lzma_raw_decode (const void *data, size_t size, unsigned prop, uint32_t dict,
                          size_t limit, size_t *consumed, mulib_buffer_t *out)
{
  mulib_lzma_model_ m; mulib_buffer_t tmp={0}; size_t used=0; int rc;
  if ((!data && size) || !consumed || !mulib_codec_buffer_valid (out)) return MULIB_CODEC_EINVAL;
  memset (&m,0,sizeof (m)); m.dictionary=dict<4096?4096:dict;
  rc=mulib_lzma_properties_ (&m,prop);
  if (!rc) { mulib_lzma_reset_ (&m); rc=mulib_lzma_decode_part_ (&m,(const unsigned char *)data,size,SIZE_MAX,0,limit,&used,&tmp); }
  if (!rc && !mulib_buffer_append (out,tmp.data,tmp.length)) rc=MULIB_CODEC_ENOMEM;
  if (!rc) *consumed=used;
  free (m.p); mulib_buffer_free (&tmp); return rc;
}
int mulib_lzma_decode (const void *data, size_t size, size_t limit, mulib_buffer_t *out)
{
  const unsigned char *p=(const unsigned char *)data; mulib_lzma_model_ m;
  mulib_buffer_t tmp={0}; size_t used=0,expected=SIZE_MAX; uint64_t length; int rc;
  if ((!data && size) || !mulib_codec_buffer_valid (out)) return MULIB_CODEC_EINVAL;
  if (size<13) return MULIB_CODEC_EFORMAT;
  length=mulib_codec_le64 (p+5);
  if (length!=UINT64_MAX) { if (length>limit || length>SIZE_MAX) return MULIB_CODEC_ELIMIT; expected=(size_t)length; }
  memset (&m,0,sizeof (m)); m.dictionary=mulib_codec_le32 (p+1); if (m.dictionary<4096) m.dictionary=4096;
  rc=mulib_lzma_properties_ (&m,p[0]);
  if (!rc) { mulib_lzma_reset_ (&m); rc=mulib_lzma_decode_part_ (&m,p+13,size-13,expected,0,limit,&used,&tmp); }
  if (!rc && used!=size-13) rc=MULIB_CODEC_EFORMAT;
  if (!rc && !mulib_buffer_append (out,tmp.data,tmp.length)) rc=MULIB_CODEC_ENOMEM;
  free (m.p); mulib_buffer_free (&tmp); return rc;
}
int mulib_lzma2_encode (const void *data, size_t size, mulib_buffer_t *out)
{
  const unsigned char *p=(const unsigned char *)data;
  mulib_buffer_t tmp={0},block={0}; size_t at=0; unsigned char end=0; int rc=0;
  if ((!data && size) || !mulib_codec_buffer_valid (out)) return MULIB_CODEC_EINVAL;
  while (at<size) {
    size_t n=size-at<32768?size-at:32768; unsigned char h[6];
    block.length=0; rc=mulib_lzma_encode_part_ (p+at,n,0,&block); if (rc) break;
    if (block.length+3<n) {
      h[0]=0xe0; h[1]=(unsigned char)((n-1)>>8); h[2]=(unsigned char)(n-1);
      h[3]=(unsigned char)((block.length-1)>>8); h[4]=(unsigned char)(block.length-1); h[5]=93;
      rc=mulib_codec_append (&tmp,h,6,SIZE_MAX);
      if (!rc) rc=mulib_codec_append (&tmp,block.data,block.length,SIZE_MAX);
    } else {
      h[0]=1; h[1]=(unsigned char)((n-1)>>8); h[2]=(unsigned char)(n-1);
      rc=mulib_codec_append (&tmp,h,3,SIZE_MAX);
      if (!rc) rc=mulib_codec_append (&tmp,p+at,n,SIZE_MAX);
    }
    if (rc) break;
    at+=n;
  }
  if (!rc) rc=mulib_codec_append (&tmp,&end,1,SIZE_MAX);
  if (!rc && !mulib_buffer_append (out,tmp.data,tmp.length)) rc=MULIB_CODEC_ENOMEM;
  mulib_buffer_free (&block); mulib_buffer_free (&tmp); return rc;
}
int mulib_lzma2_decode (const void *data, size_t size, uint32_t dictionary, size_t limit,
                       size_t *consumed, mulib_buffer_t *out)
{
  const unsigned char *p=(const unsigned char *)data; mulib_lzma_model_ m;
  mulib_buffer_t tmp={0}; size_t at=0; int ended=0,rc=MULIB_CODEC_EFORMAT,need_dict=1,need_state=1,need_prop=1;
  if ((!data && size) || !consumed || !mulib_codec_buffer_valid (out)) return MULIB_CODEC_EINVAL;
  memset (&m,0,sizeof (m)); m.dictionary=dictionary<4096?4096:dictionary;
  while (at<size) {
    unsigned c=p[at++]; size_t unpack,pack,used; rc=MULIB_CODEC_EFORMAT;
    if (!c) {
      ended=1;
      rc=mulib_buffer_append (out,tmp.data,tmp.length)?0:MULIB_CODEC_ENOMEM;
      if (!rc) *consumed=at;
      break;
    }
    if (c==1 || c>=0xe0) { m.start=tmp.length; need_dict=0; }
    if (need_dict || (c>2 && c<0x80) || size-at<2) break;
    unpack=((size_t)p[at]<<8|p[at+1])+1; at+=2;
    if (c<0x80) {
      if (unpack>size-at) break;
      rc=mulib_codec_append (&tmp,p+at,unpack,limit); if (rc) break;
      at+=unpack; need_state=1; continue;
    }
    unpack+=(size_t)(c&31)<<16;
    if (size-at<2) break;
    pack=((size_t)p[at]<<8|p[at+1])+1; at+=2;
    if (c>=0xc0) {
      if (at==size || p[at]>=225) break;
      { unsigned prop=p[at++]; if (prop%9+(prop/9)%5>4) break;
        rc=mulib_lzma_properties_ (&m,prop); if (rc) break; }
      need_prop=0;
    }
    rc=MULIB_CODEC_EFORMAT;
    if (need_prop || (need_state && c<0xa0) || pack>size-at) break;
    if (c>=0xa0) mulib_lzma_reset_ (&m);
    need_state=0;
    if (unpack>limit-tmp.length) { rc=MULIB_CODEC_ELIMIT; break; }
    rc=mulib_lzma_decode_part_ (&m,p+at,pack,unpack,1,limit,&used,&tmp); if (rc) break;
    if (used!=pack) { rc=MULIB_CODEC_EFORMAT; break; }
    at+=pack;
  }
  if (!ended && !rc) rc=MULIB_CODEC_EFORMAT;
  free (m.p); mulib_buffer_free (&tmp); return rc;
}
#endif
