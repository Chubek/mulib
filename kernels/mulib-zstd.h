/** @file mulib-zstd.h
 * @brief Dependency-free Zstandard LZ/FSE/Huffman codec and raw/RLE helpers.
 * Define MULIB_IMPL_ZSTD, CODEC_CORE and BUFFER for the full codec. Existing
 * static-inline raw helpers retain their limited profile and CT blob API. */
#ifndef INCLUDE_MULIB_ZSTD_H
#define INCLUDE_MULIB_ZSTD_H
#include "mulib-container-core.h"
#include "mulib-codec-core.h"
#ifdef __cplusplus
extern "C" {
#endif
/** @brief Append a compressed, checksummed standard frame with bounded greedy
 * LZ matching. Input may alias out; failures preserve output. */
int mulib_zstd_encode (const void *data, size_t size, mulib_buffer_t *out);
/** @brief Decode concatenated/skippable frames, all block/entropy modes and
 * content checksums within an aggregate output limit. No external dictionary. */
int mulib_zstd_decode (const void *data, size_t size, size_t limit, mulib_buffer_t *out);
/** @brief As decode, supplying a raw-content or trained Zstandard dictionary.
 * Dictionary bytes are borrowed for the call. Dictionary IDs are checked. */
int mulib_zstd_decode_dict (const void *data, size_t size, const void *dictionary,
                            size_t dictionary_size, size_t limit, mulib_buffer_t *out);
/** @brief Encode a standard frame; runs of at least eight bytes use RLE blocks.
 * Output is an initialized blob replaced atomically, and may alias input. */
static inline int
mulib_zstd_raw_encode (const void *data, size_t n, mulib_ct_blob *out, size_t limit)
{
  const unsigned char *p = (const unsigned char *)data;
  mulib_ct_blob next = { NULL, 0, 0 };
  unsigned char header[13] = { 0x28, 0xb5, 0x2f, 0xfd, 0xe0, 0, 0, 0, 0, 0, 0, 0, 0 };
  size_t at = 0;
  int rc;
  if ((!p && n) || !out) return MULIB_CT_EINVAL;
  mulib_ct_store64 (header + 5, n);
  rc = mulib_ct_blob_append (&next, header, sizeof (header), limit);
  while (!rc)
    {
      size_t run = 0, length, start = at;
      unsigned type = 0;
      uint32_t block;
      unsigned char h[3];
      while (run < n - at && run < 131072 && p[at + run] == p[at]) ++run;
      if (run >= 8) { type = 1; length = run; }
      else
        {
          length = 0;
          while (length < n - at && length < 131072)
            {
              size_t k = 1;
              while (k < 8 && k < n - at - length && p[at + length + k] == p[at + length]) ++k;
              if (k >= 8) break;
              ++length;
            }
        }
      at += length;
      block = (uint32_t)(length << 3) | (type << 1) | (at == n);
      h[0] = (unsigned char)block; h[1] = (unsigned char)(block >> 8); h[2] = (unsigned char)(block >> 16);
      rc = mulib_ct_blob_append (&next, h, 3, limit);
      if (!rc && length) rc = mulib_ct_blob_append (&next, p + start, type ? 1 : length, limit);
      if (at == n) break;
    }
  if (rc) { mulib_ct_blob_free (&next); return rc; }
  mulib_ct_blob_free (out); *out = next;
  return 0;
}
/* Walk block metadata before allocation, then repeat to copy the validated bytes. */
static inline int
mulib_zstd_raw_blocks_ (const unsigned char *p, size_t n, size_t at,
                        size_t size, unsigned char *out)
{
  size_t produced = 0;
  int last = 0;
  while (!last)
    {
      uint32_t block;
      size_t length, stored;
      unsigned type;
      if (n - at < 3) return MULIB_CT_EFORMAT;
      block = (uint32_t)p[at] | (uint32_t)p[at + 1] << 8 | (uint32_t)p[at + 2] << 16;
      at += 3; last = block & 1; type = (block >> 1) & 3; length = block >> 3;
      if (type == 2) return MULIB_CT_ENOTSUP;
      if (type == 3 || length > 131072 || length > size - produced) return MULIB_CT_EFORMAT;
      stored = type == 1 ? 1 : length;
      if (stored > n - at) return MULIB_CT_EFORMAT;
      if (out && length)
        {
          if (type) memset (out + produced, p[at], length);
          else memcpy (out + produced, p + at, length);
        }
      produced += length; at += stored;
    }
  return at == n && produced == size ? 0 : MULIB_CT_EFORMAT;
}
/** @brief Decode one complete supported frame. Bound advertised output and
 * validate every block before allocating. Input must stay immutable through
 * both passes. Reject truncation, excess bytes and content-size mismatch. */
static inline int
mulib_zstd_raw_decode (const void *data, size_t n, mulib_ct_blob *out, size_t limit)
{
  const unsigned char *p = (const unsigned char *)data;
  mulib_ct_blob next = { NULL, 0, 0 };
  uint64_t size = 0;
  size_t field, i;
  unsigned descriptor;
  int rc;
  if (!p || !out) return MULIB_CT_EINVAL;
  if (n < 5 || mulib_ct_load32 (p) != UINT32_C(0xfd2fb528)) return MULIB_CT_EFORMAT;
  descriptor = p[4];
  if (descriptor & 0x18U) return MULIB_CT_EFORMAT;
  if (!(descriptor & 0x20U) || (descriptor & 7U)) return MULIB_CT_ENOTSUP;
  field = (descriptor >> 6) == 0 ? 1 : (size_t)1 << (descriptor >> 6);
  if (n - 5 < field) return MULIB_CT_EFORMAT;
  for (i = 0; i < field; ++i) size |= (uint64_t)p[5 + i] << (8 * i);
  if (field == 2) size += 256;
  if (size > limit || size > SIZE_MAX) return MULIB_CT_ELIMIT;
  rc = mulib_zstd_raw_blocks_ (p, n, 5 + field, (size_t)size, NULL);
  if (rc) return rc;
  rc = mulib_ct_blob_reserve (&next, (size_t)size, limit);
  if (rc) return rc;
  rc = mulib_zstd_raw_blocks_ (p, n, 5 + field, (size_t)size, next.data);
  if (rc) { mulib_ct_blob_free (&next); return rc; }
  next.length = (size_t)size;
  mulib_ct_blob_free (out); *out = next;
  return 0;
}
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_ZSTD) && !defined(MULIB_ZSTD_IMPLEMENTED)
#define MULIB_ZSTD_IMPLEMENTED
typedef struct mulib_zstd_bits_ { const unsigned char *p; size_t bits; int error; } mulib_zstd_bits_;
typedef struct mulib_zstd_fse_entry_ { uint16_t base; unsigned char symbol,bits; } mulib_zstd_fse_entry_;
typedef struct mulib_zstd_fse_ { mulib_zstd_fse_entry_ entry[512]; unsigned log,valid; } mulib_zstd_fse_;
typedef struct mulib_zstd_huff_entry_ { unsigned char symbol,bits; } mulib_zstd_huff_entry_;
typedef struct mulib_zstd_state_ {
  mulib_zstd_fse_ ll,ml,of; mulib_zstd_huff_entry_ huff[2048]; unsigned hlog;
  uint32_t rep[3]; const unsigned char *dict; size_t dict_size,frame_start;
  uint64_t window;
} mulib_zstd_state_;
static unsigned mulib_zstd_log_ (uint32_t n)
{ unsigned log=0; while (n>>=1) ++log; return log; }
static int mulib_zstd_reverse_ (mulib_zstd_bits_ *b, const unsigned char *p, size_t n)
{
  unsigned last;
  if (!n || !p[n-1] || n>SIZE_MAX/8) return 0;
  last=mulib_zstd_log_ (p[n-1]); b->p=p; b->bits=(n-1)*8+last; b->error=0; return 1;
}
static uint32_t mulib_zstd_read_ (mulib_zstd_bits_ *b, unsigned n)
{
  uint32_t v=0; unsigned i;
  if (n>32 || n>b->bits) { b->error=1; return 0; }
  b->bits-=n;
  for (i=0;i<n;++i) { size_t bit=b->bits+i; v|=(uint32_t)((b->p[bit/8]>>(bit&7))&1u)<<i; }
  return v;
}
static uint32_t mulib_zstd_forward_ (const unsigned char *p, size_t n, size_t *at, unsigned count, int *error)
{
  uint32_t v=0; unsigned i;
  if (n>SIZE_MAX/8 || count>32 || *at>n*8 || count>n*8-*at) { *error=1; return 0; }
  for (i=0;i<count;++i) { size_t bit=(*at)++; v|=(uint32_t)((p[bit/8]>>(bit&7))&1u)<<i; }
  return v;
}
static int mulib_zstd_fse_build_ (mulib_zstd_fse_ *t, const int *norm, unsigned symbols, unsigned log)
{
  unsigned size=1u<<log,next[256],pos=0,step=(size>>1)+(size>>3)+3,i,s,total=0;
  int high=(int)size-1;
  if (log>9 || symbols>256) return MULIB_CODEC_EFORMAT;
  for (s=0;s<symbols;++s) {
    if (norm[s]<-1) return MULIB_CODEC_EFORMAT;
    total+=(unsigned)(norm[s]<0?1:norm[s]);
    if (total>size) return MULIB_CODEC_EFORMAT;
    next[s]=(unsigned)(norm[s]<0?1:norm[s]);
    if (norm[s]<0) t->entry[high--].symbol=(unsigned char)s;
  }
  if (total!=size) return MULIB_CODEC_EFORMAT;
  for (s=0;s<symbols;++s) for (i=0;i<(unsigned)(norm[s]>0?norm[s]:0);++i) {
    t->entry[pos].symbol=(unsigned char)s;
    do { pos=(pos+step)&(size-1); } while ((int)pos>high);
  }
  if (pos) return MULIB_CODEC_EFORMAT;
  for (i=0;i<size;++i) {
    unsigned value=next[t->entry[i].symbol]++,bits=log-mulib_zstd_log_ (value);
    t->entry[i].bits=(unsigned char)bits; t->entry[i].base=(uint16_t)((value<<bits)-size);
  }
  t->log=log; t->valid=1; return 0;
}
static int mulib_zstd_fse_description_ (mulib_zstd_fse_ *t, const unsigned char *p, size_t n,
                                      unsigned max_symbol, unsigned max_log, size_t *used)
{
  int norm[256]={0},error=0,remaining,threshold; unsigned log,nb,s=0,nonzero=0; size_t at=0;
  log=mulib_zstd_forward_ (p,n,&at,4,&error)+5;
  if (error || log>max_log) return MULIB_CODEC_EFORMAT;
  remaining=(1<<log)+1; threshold=1<<log; nb=log+1;
  while (remaining>1 && s<=max_symbol) {
    int count,max=2*threshold-1-remaining; size_t peek=at;
    count=(int)mulib_zstd_forward_ (p,n,&peek,nb-1,&error);
    if (count<max) at=peek;
    else { count=(int)mulib_zstd_forward_ (p,n,&at,nb,&error); if (count>=threshold) count-=max; }
    if (error) return MULIB_CODEC_EFORMAT;
    --count; norm[s++]=count; if (count) ++nonzero;
    remaining-=count<0?-count:count;
    if (remaining<1) return MULIB_CODEC_EFORMAT;
    if (!count) {
      unsigned repeat;
      do {
        repeat=mulib_zstd_forward_ (p,n,&at,2,&error);
        if (error || repeat>max_symbol+1-s) return MULIB_CODEC_EFORMAT;
        s+=repeat;
      } while (repeat==3);
    }
    while (remaining<threshold) { threshold>>=1; --nb; }
  }
  if (remaining!=1 || nonzero<2) return MULIB_CODEC_EFORMAT;
  *used=(at+7)/8; return mulib_zstd_fse_build_ (t,norm,s,log);
}
static const int mulib_zstd_ll_norm_[36]={4,3,2,2,2,2,2,2,2,2,2,2,2,1,1,1,2,2,2,2,2,2,2,2,2,3,2,1,1,1,1,1,-1,-1,-1,-1};
static const int mulib_zstd_ml_norm_[53]={1,4,3,2,2,2,2,2,2,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,-1,-1,-1,-1,-1,-1,-1};
static const int mulib_zstd_of_norm_[29]={1,1,1,1,1,1,2,2,2,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,-1,-1,-1,-1,-1};
static const uint32_t mulib_zstd_ll_base_[36]={0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,18,20,22,24,28,32,40,48,64,128,256,512,1024,2048,4096,8192,16384,32768,65536};
static const unsigned char mulib_zstd_ll_bits_[36]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,2,2,3,3,4,6,7,8,9,10,11,12,13,14,15,16};
static const uint32_t mulib_zstd_ml_base_[53]={3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,37,39,41,43,47,51,59,67,83,99,131,259,515,1027,2051,4099,8195,16387,32771,65539};
static const unsigned char mulib_zstd_ml_bits_[53]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,2,2,3,3,4,4,5,7,8,9,10,11,12,13,14,15,16};
static int mulib_zstd_huff_description_ (mulib_zstd_state_ *s, const unsigned char *p, size_t n, size_t *used)
{
  unsigned weights[256],count=0,i,log,total=0,remaining,offset=0; size_t size;
  if (!n) return MULIB_CODEC_EFORMAT;
  size=p[0];
  if (size>=128) {
    count=(unsigned)size-127; size=(count+1)/2;
    if (size>n-1) return MULIB_CODEC_EFORMAT;
    for (i=0;i<count;++i) weights[i]=(p[1+i/2]>>(i&1?0:4))&15;
    *used=size+1;
  } else {
    mulib_zstd_fse_ table; mulib_zstd_bits_ b; unsigned state[2],turn=0; size_t header; int rc;
    if (!size || size>n-1) return MULIB_CODEC_EFORMAT;
    rc=mulib_zstd_fse_description_ (&table,p+1,size,11,6,&header); if (rc) return rc;
    if (!mulib_zstd_reverse_ (&b,p+1+header,size-header)) return MULIB_CODEC_EFORMAT;
    state[0]=mulib_zstd_read_ (&b,table.log); state[1]=mulib_zstd_read_ (&b,table.log);
    if (b.error) return MULIB_CODEC_EFORMAT;
    for (;;) {
      mulib_zstd_fse_entry_ entry=table.entry[state[turn]];
      if (count>=254) return MULIB_CODEC_EFORMAT;
      weights[count++]=entry.symbol;
      if (entry.bits>b.bits) { weights[count++]=table.entry[state[turn^1]].symbol; break; }
      state[turn]=entry.base+mulib_zstd_read_ (&b,entry.bits); turn^=1;
    }
    *used=size+1;
  }
  for (i=0;i<count;++i) { if (weights[i]>11) return MULIB_CODEC_EFORMAT; if (weights[i]) total+=1u<<(weights[i]-1); }
  if (!total) return MULIB_CODEC_EFORMAT;
  log=mulib_zstd_log_ (total)+1; if (log>11) return MULIB_CODEC_EFORMAT;
  remaining=(1u<<log)-total;
  if (!remaining || (remaining&(remaining-1))) return MULIB_CODEC_EFORMAT;
  weights[count++]=mulib_zstd_log_ (remaining)+1;
  for (i=1;i<=log;++i) {
    unsigned symbol;
    for (symbol=0;symbol<count;++symbol) if (weights[symbol]==i) {
      unsigned k,width=1u<<(i-1);
      if (width>(1u<<log)-offset) return MULIB_CODEC_EFORMAT;
      for (k=0;k<width;++k) { s->huff[offset+k].symbol=(unsigned char)symbol; s->huff[offset+k].bits=(unsigned char)(log+1-i); }
      offset+=width;
    }
  }
  if (offset!=(1u<<log)) return MULIB_CODEC_EFORMAT;
  s->hlog=log; return 0;
}
static int mulib_zstd_huff_stream_ (mulib_zstd_state_ *s, const unsigned char *p, size_t n,
                                   unsigned char *out, size_t length)
{
  mulib_zstd_bits_ b; size_t i;
  if (!mulib_zstd_reverse_ (&b,p,n)) return MULIB_CODEC_EFORMAT;
  for (i=0;i<length;++i) {
    mulib_zstd_bits_ peek=b; unsigned bits=b.bits<s->hlog?(unsigned)b.bits:s->hlog;
    unsigned index=mulib_zstd_read_ (&peek,bits)<<(s->hlog-bits); mulib_zstd_huff_entry_ e=s->huff[index];
    if (!e.bits || e.bits>b.bits) return MULIB_CODEC_EFORMAT;
    out[i]=e.symbol; b.bits-=e.bits;
  }
  return b.bits?MULIB_CODEC_EFORMAT:0;
}
static int mulib_zstd_literals_ (mulib_zstd_state_ *s, const unsigned char *p, size_t n,
                                mulib_buffer_t *literal, size_t *used, size_t maximum)
{
  unsigned type,format,streams=1; size_t hlen,length,packed,tree=0,i; uint64_t header=0; int rc;
  if (!n) return MULIB_CODEC_EFORMAT;
  type=p[0]&3; format=(p[0]>>2)&3;
  if (type<2) {
    hlen=(format&1)?(format==1?2:3):1;
    if (hlen>n) return MULIB_CODEC_EFORMAT;
    for (i=0;i<hlen;++i) header|=(uint64_t)p[i]<<(8*i);
    length=(size_t)(header>>(hlen==1?3:4)); packed=type?1:length;
  } else {
    hlen=format<2?3:format+2; streams=format?4:1;
    if (hlen>n) return MULIB_CODEC_EFORMAT;
    for (i=0;i<hlen;++i) header|=(uint64_t)p[i]<<(8*i);
    { unsigned bits=format<2?10:format==2?14:18;
      length=(size_t)((header>>4)&(((uint64_t)1<<bits)-1)); packed=(size_t)(header>>(4+bits)); }
  }
  if (length>maximum || packed>n-hlen) return MULIB_CODEC_EFORMAT;
  literal->length=0; rc=mulib_codec_reserve (literal,length,maximum); if (rc) return rc;
  if (type<2) {
    if (length) { if (type) memset (literal->data,p[hlen],length); else memcpy (literal->data,p+hlen,length); }
  } else {
    const unsigned char *payload=p+hlen; size_t available=packed;
    if (type==2) { rc=mulib_zstd_huff_description_ (s,payload,available,&tree); if (rc) return rc; }
    else if (!s->hlog) return MULIB_CODEC_EFORMAT;
    payload+=tree; available-=tree;
    if (streams==1) { rc=mulib_zstd_huff_stream_ (s,payload,available,literal->data,length); if (rc) return rc; }
    else {
      size_t sizes[4],segment=(length+3)/4,at=6,produced=0;
      if (available<6 || segment*3>length) return MULIB_CODEC_EFORMAT;
      for (i=0;i<3;++i) sizes[i]=payload[i*2]|(size_t)payload[i*2+1]<<8;
      if (sizes[0]+sizes[1]+sizes[2]>available-6) return MULIB_CODEC_EFORMAT;
      sizes[3]=available-6-sizes[0]-sizes[1]-sizes[2];
      for (i=0;i<4;++i) {
        size_t count=i==3?length-produced:segment;
        rc=mulib_zstd_huff_stream_ (s,payload+at,sizes[i],literal->data?literal->data+produced:NULL,count); if (rc) return rc;
        at+=sizes[i]; produced+=count;
      }
    }
  }
  literal->length=length; *used=hlen+packed; return 0;
}
static int mulib_zstd_table_ (mulib_zstd_fse_ *t, unsigned mode, unsigned type,
                             const unsigned char *p, size_t n, size_t *used)
{
  unsigned maximum=type==0?35:type==1?31:52;
  *used=0;
  if (mode==3) return t->valid?0:MULIB_CODEC_EFORMAT;
  if (mode==0) return mulib_zstd_fse_build_ (t,type==0?mulib_zstd_ll_norm_:type==1?mulib_zstd_of_norm_:mulib_zstd_ml_norm_,type==0?36:type==1?29:53,type==1?5:6);
  if (mode==2) return mulib_zstd_fse_description_ (t,p,n,maximum,type==1?8:9,used);
  if (!n || p[0]>maximum) return MULIB_CODEC_EFORMAT;
  t->log=0; t->valid=1; t->entry[0].base=0; t->entry[0].bits=0; t->entry[0].symbol=p[0]; *used=1; return 0;
}
static int mulib_zstd_match_ (mulib_zstd_state_ *s, mulib_buffer_t *out, uint64_t distance, size_t len, size_t limit)
{
  size_t produced=out->length-s->frame_start,i; int rc;
  if (!distance || distance>(uint64_t)produced+s->dict_size
      || (distance>s->window && (produced>=s->window || !s->dict_size))) return MULIB_CODEC_EFORMAT;
  if (len>limit-out->length) return MULIB_CODEC_ELIMIT;
  rc=mulib_codec_reserve (out,out->length+len,limit); if (rc) return rc;
  for (i=0;i<len;++i) {
    unsigned char c;
    if (distance>produced) c=s->dict[s->dict_size-(size_t)(distance-produced)];
    else c=out->data[out->length-(size_t)distance];
    out->data[out->length++]=c; ++produced;
  }
  return 0;
}
static int mulib_zstd_block_ (mulib_zstd_state_ *s, const unsigned char *p, size_t n,
                             size_t maximum, size_t limit, mulib_buffer_t *out)
{
  mulib_buffer_t literal={0}; mulib_zstd_bits_ b; size_t at,used,li=0,before=out->length;
  unsigned count,modes,state_ll,state_ml,state_of,i; int rc;
  rc=mulib_zstd_literals_ (s,p,n,&literal,&at,maximum); if (rc) goto done;
  rc=MULIB_CODEC_EFORMAT; if (at==n) goto done;
  count=p[at++];
  if (count>=128) {
    if (at==n) goto done;
    if (count<255) count=((count-128)<<8)+p[at++];
    else { if (n-at<2) goto done; count=0x7f00+p[at]+((unsigned)p[at+1]<<8); at+=2; }
  }
  if (!count) { if (at!=n) goto done; rc=mulib_codec_append (out,literal.data,literal.length,limit); goto done; }
  if (at==n) goto done;
  modes=p[at++]; if (modes&3) goto done;
  rc=mulib_zstd_table_ (&s->ll,modes>>6,0,p+at,n-at,&used); if (rc) goto done; at+=used;
  rc=mulib_zstd_table_ (&s->of,(modes>>4)&3,1,p+at,n-at,&used); if (rc) goto done; at+=used;
  rc=mulib_zstd_table_ (&s->ml,(modes>>2)&3,2,p+at,n-at,&used); if (rc) goto done; at+=used;
  rc=MULIB_CODEC_EFORMAT; if (!mulib_zstd_reverse_ (&b,p+at,n-at)) goto done;
  state_ll=mulib_zstd_read_ (&b,s->ll.log); state_of=mulib_zstd_read_ (&b,s->of.log); state_ml=mulib_zstd_read_ (&b,s->ml.log);
  for (i=0;i<count;++i) {
    mulib_zstd_fse_entry_ ll=s->ll.entry[state_ll],ml=s->ml.entry[state_ml],of=s->of.entry[state_of];
    uint64_t offset=((uint64_t)1<<of.symbol)+mulib_zstd_read_ (&b,of.symbol),distance;
    size_t match=mulib_zstd_ml_base_[ml.symbol]+mulib_zstd_read_ (&b,mulib_zstd_ml_bits_[ml.symbol]);
    size_t literals=mulib_zstd_ll_base_[ll.symbol]+mulib_zstd_read_ (&b,mulib_zstd_ll_bits_[ll.symbol]);
    if (b.error || literals>literal.length-li || literals>maximum-(out->length-before)) goto done;
    if (offset>3) { distance=offset-3; s->rep[2]=s->rep[1]; s->rep[1]=s->rep[0]; s->rep[0]=(uint32_t)distance; }
    else {
      unsigned index=(unsigned)offset-1+(literals==0);
      if (index==3) { distance=(uint64_t)s->rep[0]-1; s->rep[2]=s->rep[1]; s->rep[1]=s->rep[0]; s->rep[0]=(uint32_t)distance; }
      else { unsigned j; distance=s->rep[index]; for (j=index;j>0;--j) s->rep[j]=s->rep[j-1]; s->rep[0]=(uint32_t)distance; }
    }
    rc=mulib_codec_append (out,literal.data?literal.data+li:NULL,literals,limit); if (rc) goto done; li+=literals;
    rc=MULIB_CODEC_EFORMAT;
    if (match>maximum-(out->length-before)) goto done;
    rc=mulib_zstd_match_ (s,out,distance,match,limit); if (rc) goto done;
    rc=MULIB_CODEC_EFORMAT;
    if (i+1<count) {
      state_ll=ll.base+mulib_zstd_read_ (&b,ll.bits);
      state_ml=ml.base+mulib_zstd_read_ (&b,ml.bits);
      state_of=of.base+mulib_zstd_read_ (&b,of.bits);
      if (b.error) goto done;
    }
  }
  if (b.bits || b.error || literal.length-li>maximum-(out->length-before)) goto done;
  rc=mulib_codec_append (out,literal.data?literal.data+li:NULL,literal.length-li,limit);
done:
  mulib_buffer_free (&literal); return rc;
}
static int mulib_zstd_dictionary_ (mulib_zstd_state_ *s, const unsigned char *p, size_t n, uint32_t id)
{
  size_t at=8,used; int rc;
  s->rep[0]=1; s->rep[1]=4; s->rep[2]=8;
  if (n<4 || mulib_codec_le32 (p)!=UINT32_C(0xec30a437)) {
    if (id) return MULIB_CODEC_EUNSUPPORTED;
    s->dict=p; s->dict_size=n; return 0;
  }
  if (n<8 || (id && mulib_codec_le32 (p+4)!=id)) return MULIB_CODEC_EUNSUPPORTED;
  rc=mulib_zstd_huff_description_ (s,p+at,n-at,&used); if (rc) return rc; at+=used;
  rc=mulib_zstd_fse_description_ (&s->of,p+at,n-at,31,8,&used); if (rc) return rc; at+=used;
  rc=mulib_zstd_fse_description_ (&s->ml,p+at,n-at,52,9,&used); if (rc) return rc; at+=used;
  rc=mulib_zstd_fse_description_ (&s->ll,p+at,n-at,35,9,&used); if (rc) return rc; at+=used;
  if (n-at<12) return MULIB_CODEC_EFORMAT;
  { unsigned i; for (i=0;i<3;++i) { s->rep[i]=mulib_codec_le32 (p+at+i*4); if (!s->rep[i] || s->rep[i]>n-at-12) return MULIB_CODEC_EFORMAT; } }
  s->dict=p+at+12; s->dict_size=n-at-12; return 0;
}
int mulib_zstd_decode_dict (const void *data, size_t size, const void *dictionary, size_t dictionary_size,
                            size_t limit, mulib_buffer_t *out)
{
  const unsigned char *p=(const unsigned char *)data; mulib_buffer_t tmp={0}; mulib_zstd_state_ *s;
  size_t at=0; int rc=MULIB_CODEC_EFORMAT;
  if ((!data && size) || (!dictionary && dictionary_size) || !mulib_codec_buffer_valid (out)) return MULIB_CODEC_EINVAL;
  if (!size) return rc;
  s=(mulib_zstd_state_ *)malloc (sizeof (*s)); if (!s) return MULIB_CODEC_ENOMEM;
  while (at<size) {
    uint32_t magic,id=0; unsigned descriptor,single,fcs,ds,i,last=0; uint64_t content=UINT64_MAX;
    size_t start=tmp.length,maximum;
    rc=MULIB_CODEC_EFORMAT;
    if (size-at<4) goto done;
    magic=mulib_codec_le32 (p+at); at+=4;
    if ((magic&UINT32_C(0xfffffff0))==UINT32_C(0x184d2a50)) {
      size_t n; if (size-at<4) goto done; n=mulib_codec_le32 (p+at); at+=4;
      if (n>size-at) goto done;
      at+=n; continue;
    }
    if (magic!=UINT32_C(0xfd2fb528) || at==size) goto done;
    memset (s,0,sizeof (*s)); s->frame_start=start;
    descriptor=p[at++]; if (descriptor&8) goto done;
    single=descriptor&32; fcs=descriptor>>6; ds=descriptor&3;
    if (!single) {
      unsigned wd; if (at==size) goto done; wd=p[at++];
      s->window=((uint64_t)1<<(10+(wd>>3)))*(8+(wd&7))/8;
    }
    ds=ds==3?4:ds;
    if (ds>size-at) goto done;
    for (i=0;i<ds;++i) id|=(uint32_t)p[at++]<<(8*i);
    fcs=fcs?(1u<<fcs):(single?1:0);
    if (fcs>size-at) goto done;
    if (fcs) {
      content=0; for (i=0;i<fcs;++i) content|=(uint64_t)p[at++]<<(8*i);
      if (fcs==2) content+=256;
      if (content>limit-tmp.length) { rc=MULIB_CODEC_ELIMIT; goto done; }
    }
    if (single) s->window=content;
    maximum=s->window<131072?(size_t)s->window:131072;
    rc=mulib_zstd_dictionary_ (s,(const unsigned char *)dictionary,dictionary_size,id); if (rc) goto done;
    while (!last) {
      uint32_t h; unsigned type; size_t n,stored,before=tmp.length;
      rc=MULIB_CODEC_EFORMAT;
      if (size-at<3) goto done;
      h=p[at]|(uint32_t)p[at+1]<<8|(uint32_t)p[at+2]<<16; at+=3;
      last=h&1; type=(h>>1)&3; n=h>>3; stored=type==1?1:n;
      if (type==3 || n>maximum || stored>size-at) goto done;
      if (type==2) rc=mulib_zstd_block_ (s,p+at,n,maximum,limit,&tmp);
      else if (type==0) rc=mulib_codec_append (&tmp,p+at,n,limit);
      else {
        if (n>limit-tmp.length) { rc=MULIB_CODEC_ELIMIT; goto done; }
        rc=mulib_codec_reserve (&tmp,tmp.length+n,limit);
        if (!rc) { if (n) memset (tmp.data+tmp.length,p[at],n); tmp.length+=n; }
      }
      if (rc) goto done;
      if (tmp.length-before>maximum || (content!=UINT64_MAX && tmp.length-start>content)) { rc=MULIB_CODEC_EFORMAT; goto done; }
      at+=stored;
    }
    rc=MULIB_CODEC_EFORMAT;
    if (content!=UINT64_MAX && tmp.length-start!=content) goto done;
    if (descriptor&4) {
      if (size-at<4) goto done;
      if ((uint32_t)mulib_codec_xxh64 (tmp.data?tmp.data+start:NULL,tmp.length-start)!=mulib_codec_le32 (p+at)) { rc=MULIB_CODEC_ECHECKSUM; goto done; }
      at+=4;
    }
  }
  rc=mulib_buffer_append (out,tmp.data,tmp.length)?0:MULIB_CODEC_ENOMEM;
done:
  free (s); mulib_buffer_free (&tmp); return rc;
}
int mulib_zstd_decode (const void *data, size_t size, size_t limit, mulib_buffer_t *out)
{ return mulib_zstd_decode_dict (data,size,NULL,0,limit,out); }
static int mulib_zstd_write_bits_ (mulib_buffer_t *b, uint32_t value, unsigned n, unsigned *bits, unsigned *count)
{
  unsigned i;
  for (i=0;i<n;++i) {
    *bits|=((value>>i)&1u)<<*count;
    if (++*count==8) { unsigned char c=(unsigned char)*bits; int rc=mulib_codec_append (b,&c,1,SIZE_MAX); if (rc) return rc; *bits=*count=0; }
  }
  return 0;
}
int mulib_zstd_encode (const void *data, size_t size, mulib_buffer_t *out)
{
  const unsigned char *p=(const unsigned char *)data; mulib_buffer_t tmp={0},block={0};
  unsigned char header[13]={0x28,0xb5,0x2f,0xfd,0xe4,0,0,0,0,0,0,0,0};
  size_t *last,at=0,i; int rc;
  if ((!data && size) || !mulib_codec_buffer_valid (out)) return MULIB_CODEC_EINVAL;
  last=(size_t *)malloc (16384*sizeof (*last)); if (!last) return MULIB_CODEC_ENOMEM;
  for (i=0;i<16384;++i) last[i]=SIZE_MAX;
  mulib_codec_put64 (header+5,size); rc=mulib_codec_append (&tmp,header,13,SIZE_MAX);
  do {
    size_t end=size-at<131072?size:at+131072,pos=at,match=0,prev=0,n,lit;
    unsigned type=0; unsigned char bh[3]; uint32_t h;
    if (rc) break;
    while (end-pos>=4) {
      unsigned hash=(unsigned)((mulib_codec_le32 (p+pos)*UINT32_C(2654435761))>>18);
      prev=last[hash]; last[hash]=pos;
      if (prev!=SIZE_MAX && pos-prev<=UINT32_MAX-3) {
        while (match<end-pos && p[prev+match]==p[pos+match]) ++match;
        if (match>=8) break;
      }
      match=0; ++pos;
    }
    n=match?pos-at+match:end-at; lit=match?pos-at:n; block.length=0;
    if (match) {
      unsigned lc=0,mc=0,oc,bits=0,count=0; uint32_t offset=(uint32_t)(pos-prev+3); unsigned char sh[5];
      while (lc<35 && mulib_zstd_ll_base_[lc+1]<=lit) ++lc;
      while (mc<52 && mulib_zstd_ml_base_[mc+1]<=match) ++mc;
      oc=mulib_zstd_log_ (offset);
      if (lit<32) { sh[0]=(unsigned char)(lit<<3); rc=mulib_codec_append (&block,sh,1,SIZE_MAX); }
      else if (lit<4096) { uint32_t v=(uint32_t)(lit<<4)|4; sh[0]=(unsigned char)v; sh[1]=(unsigned char)(v>>8); rc=mulib_codec_append (&block,sh,2,SIZE_MAX); }
      else { uint32_t v=(uint32_t)(lit<<4)|12; sh[0]=(unsigned char)v; sh[1]=(unsigned char)(v>>8); sh[2]=(unsigned char)(v>>16); rc=mulib_codec_append (&block,sh,3,SIZE_MAX); }
      if (!rc) rc=mulib_codec_append (&block,p+at,lit,SIZE_MAX);
      sh[0]=1; sh[1]=0x54; sh[2]=(unsigned char)lc; sh[3]=(unsigned char)oc; sh[4]=(unsigned char)mc;
      if (!rc) rc=mulib_codec_append (&block,sh,5,SIZE_MAX);
      if (!rc) rc=mulib_zstd_write_bits_ (&block,(uint32_t)lit-mulib_zstd_ll_base_[lc],mulib_zstd_ll_bits_[lc],&bits,&count);
      if (!rc) rc=mulib_zstd_write_bits_ (&block,(uint32_t)match-mulib_zstd_ml_base_[mc],mulib_zstd_ml_bits_[mc],&bits,&count);
      if (!rc) rc=mulib_zstd_write_bits_ (&block,offset-((uint32_t)1<<oc),oc,&bits,&count);
      if (!rc) rc=mulib_zstd_write_bits_ (&block,1,1,&bits,&count);
      if (!rc && count) { unsigned char c=(unsigned char)bits; rc=mulib_codec_append (&block,&c,1,SIZE_MAX); }
      if (block.length<n) type=2;
    }
    if (rc) break;
    h=(uint32_t)((type?block.length:n)<<3)|(type<<1)|(at+n==size);
    bh[0]=(unsigned char)h; bh[1]=(unsigned char)(h>>8); bh[2]=(unsigned char)(h>>16);
    rc=mulib_codec_append (&tmp,bh,3,SIZE_MAX);
    if (!rc) rc=mulib_codec_append (&tmp,type?block.data:(p?p+at:NULL),type?block.length:n,SIZE_MAX);
    at+=n;
  } while (at<size);
  if (!rc) { unsigned char crc[4]; mulib_codec_put32 (crc,(uint32_t)mulib_codec_xxh64 (data,size)); rc=mulib_codec_append (&tmp,crc,4,SIZE_MAX); }
  if (!rc && !mulib_buffer_append (out,tmp.data,tmp.length)) rc=MULIB_CODEC_ENOMEM;
  free (last); mulib_buffer_free (&block); mulib_buffer_free (&tmp); return rc;
}
#endif
