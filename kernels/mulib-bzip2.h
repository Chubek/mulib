/** @file mulib-bzip2.h
 * @brief Dependency-free bzip2 BWT/MTF/RLE/Huffman compression and decompression.
 * Define MULIB_IMPL_BZIP2, CODEC_CORE and BUFFER.
 */
#ifndef MULIB_BZIP2_H
#define MULIB_BZIP2_H
#include "mulib-codec-core.h"
#ifdef __cplusplus
extern "C" {
#endif
/** @brief Append a BZh1 stream using 64 KiB input blocks and canonical Huffman
 * coding. O(n log n) cyclic suffix sorting; no global comparator context. */
int mulib_bzip2_encode (const void *data, size_t size, mulib_buffer_t *out);
/** @brief Decode concatenated BZh1..BZh9 streams and verify block/stream CRCs.
 * limit bounds aggregate output. Failure is atomic; input may alias output. */
int mulib_bzip2_decode (const void *data, size_t size, size_t limit, mulib_buffer_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_BZIP2) && !defined(MULIB_BZIP2_IMPLEMENTED)
#define MULIB_BZIP2_IMPLEMENTED
static uint32_t mulib_bzip2_crc_ (const unsigned char *p, size_t n)
{
  uint32_t c=UINT32_MAX; size_t i; unsigned k;
  for (i=0;i<n;++i) { c^=(uint32_t)p[i]<<24; for (k=0;k<8;++k) c=(c<<1)^((c>>31)?UINT32_C(0x04c11db7):0); }
  return ~c;
}
/* Stable counting sorts of cyclic suffix equivalence classes. */
static int mulib_bzip2_bwt_ (const unsigned char *p, size_t n, unsigned char *last, uint32_t *origin)
{
  uint32_t *mem,*order,*rank,*next,*work,*count; size_t i,k,classes=256;
  if (n>SIZE_MAX/(5*sizeof (*mem))) return MULIB_CODEC_ELIMIT;
  mem=(uint32_t *)calloc (5*(n<256?256:n),sizeof (*mem)); if (!mem) return MULIB_CODEC_ENOMEM;
  order=mem; rank=order+(n<256?256:n); next=rank+(n<256?256:n);
  work=next+(n<256?256:n); count=work+(n<256?256:n);
  for (i=0;i<n;++i) ++count[p[i]];
  for (i=1;i<256;++i) count[i]+=count[i-1];
  for (i=n;i>0;--i) order[--count[p[i-1]]]=(uint32_t)(i-1);
  for (i=0;i<n;++i) rank[i]=p[i];
  for (k=1;k<n;k*=2) {
    size_t nc=1;
    memset (count,0,classes*sizeof (*count));
    for (i=0;i<n;++i) { work[i]=(uint32_t)(order[i]>=k?order[i]-k:order[i]+n-k); ++count[rank[work[i]]]; }
    for (i=1;i<classes;++i) count[i]+=count[i-1];
    for (i=n;i>0;--i) order[--count[rank[work[i-1]]]]=work[i-1];
    next[order[0]]=0;
    for (i=1;i<n;++i) {
      size_t a=order[i-1],b=order[i];
      if (rank[a]!=rank[b] || rank[(a+k)%n]!=rank[(b+k)%n]) ++nc;
      next[b]=(uint32_t)(nc-1);
    }
    { uint32_t *swap=rank; rank=next; next=swap; } classes=nc;
    if (classes==n || k>n/2) break;
  }
  for (i=0;i<n;++i) { if (!order[i]) *origin=(uint32_t)i; last[i]=p[order[i]?order[i]-1:n-1]; }
  free (mem); return 0;
}
static int mulib_bzip2_emit_run_ (mulib_buffer_t *symbols, size_t run)
{
  while (run) {
    uint16_t s=(uint16_t)((run-1)&1); int rc=mulib_codec_append (symbols,&s,sizeof (s),SIZE_MAX);
    if (rc) return rc;
    run=(run-1)>>1;
  }
  return 0;
}
static int mulib_bzip2_encode_block_ (mulib_codec_writer_t *w, const unsigned char *p,
                                     size_t n, uint32_t *combined)
{
  mulib_buffer_t rle={0},symbols={0}; unsigned char *last=NULL,mtf[256],used[256]={0};
  uint32_t crc=mulib_bzip2_crc_ (p,n),origin=0; size_t at=0,i,run=0,ns,selectors;
  unsigned alphabet=0,groups=0; int rc=0;
  while (at<n) {
    size_t count=1; unsigned char c=p[at];
    while (count<n-at && count<259 && p[at+count]==c) ++count;
    for (i=0;i<(count<4?count:4);++i) { rc=mulib_codec_append (&rle,&c,1,100000); if (rc) goto done; }
    if (count>=4) { unsigned char extra=(unsigned char)(count-4); rc=mulib_codec_append (&rle,&extra,1,100000); if (rc) goto done; }
    at+=count;
  }
  last=(unsigned char *)malloc (rle.length); if (!last) { rc=MULIB_CODEC_ENOMEM; goto done; }
  rc=mulib_bzip2_bwt_ (rle.data,rle.length,last,&origin); if (rc) goto done;
  for (i=0;i<rle.length;++i) used[last[i]]=1;
  for (i=0;i<256;++i) if (used[i]) mtf[alphabet++]=(unsigned char)i;
  for (i=0;i<rle.length;++i) {
    unsigned index=0; while (mtf[index]!=last[i]) ++index;
    if (!index) { ++run; continue; }
    rc=mulib_bzip2_emit_run_ (&symbols,run); if (rc) goto done; run=0;
    { uint16_t s=(uint16_t)(index+1); unsigned char c=mtf[index];
      memmove (mtf+1,mtf,index); mtf[0]=c;
      rc=mulib_codec_append (&symbols,&s,sizeof (s),SIZE_MAX); if (rc) goto done; }
  }
  rc=mulib_bzip2_emit_run_ (&symbols,run); if (rc) goto done;
  { uint16_t end=(uint16_t)(alphabet+1); rc=mulib_codec_append (&symbols,&end,sizeof (end),SIZE_MAX); if (rc) goto done; }
  ns=symbols.length/2; selectors=(ns+49)/50;
#define MULIB_BZ_PUT_(v,b) do { rc=mulib_codec_put (w,(uint32_t)(v),(b)); if (rc) goto done; } while (0)
  MULIB_BZ_PUT_(0x314159,24); MULIB_BZ_PUT_(0x265359,24); MULIB_BZ_PUT_(crc,32);
  MULIB_BZ_PUT_(0,1); MULIB_BZ_PUT_(origin,24);
  for (i=0;i<16;++i) { unsigned j,any=0; for (j=0;j<16;++j) any|=used[i*16+j]; groups=(groups<<1)|any; }
  MULIB_BZ_PUT_(groups,16);
  for (i=0;i<16;++i) if (groups&(1u<<(15-i))) { unsigned j; for (j=0;j<16;++j) MULIB_BZ_PUT_(used[i*16+j],1); }
  MULIB_BZ_PUT_(2,3); MULIB_BZ_PUT_(selectors,15);
  for (i=0;i<selectors;++i) MULIB_BZ_PUT_(0,1);
  /* Two identical complete fixed trees: unused leaves are legal in bzip2. */
  for (i=0;i<2;++i) { unsigned j; MULIB_BZ_PUT_(9,5); for (j=0;j<alphabet+2;++j) MULIB_BZ_PUT_(0,1); }
  for (i=0;i<ns;++i) { uint16_t s; memcpy (&s,symbols.data+i*2,2); MULIB_BZ_PUT_(s,9); }
  *combined=(*combined<<1|*combined>>31)^crc;
#undef MULIB_BZ_PUT_
done:
  free (last); mulib_buffer_free (&rle); mulib_buffer_free (&symbols); return rc;
}
int mulib_bzip2_encode (const void *data, size_t size, mulib_buffer_t *out)
{
  const unsigned char *p=(const unsigned char *)data; mulib_codec_writer_t w;
  size_t at=0; uint32_t crc=0; int rc;
  if ((!data && size) || !mulib_codec_buffer_valid (out)) return MULIB_CODEC_EINVAL;
  memset (&w,0,sizeof (w)); rc=mulib_codec_append (&w.bytes,"BZh1",4,SIZE_MAX);
  while (!rc && at<size) { size_t n=size-at<64000?size-at:64000; rc=mulib_bzip2_encode_block_ (&w,p+at,n,&crc); at+=n; }
  if (!rc) rc=mulib_codec_put (&w,0x177245,24);
  if (!rc) rc=mulib_codec_put (&w,0x385090,24);
  if (!rc) rc=mulib_codec_put (&w,crc,32);
  if (!rc) rc=mulib_codec_flush (&w);
  if (!rc && !mulib_buffer_append (out,w.bytes.data,w.bytes.length)) rc=MULIB_CODEC_ENOMEM;
  mulib_buffer_free (&w.bytes); return rc;
}
typedef struct mulib_bzip2_tree_ { unsigned count[21],first[21],index[21],symbol[258]; } mulib_bzip2_tree_;
static int mulib_bzip2_make_tree_ (mulib_bzip2_tree_ *t, const unsigned *len, unsigned n)
{
  unsigned i,offset[21],code=0,total=0; int left=1;
  memset (t,0,sizeof (*t));
  for (i=0;i<n;++i) { if (!len[i] || len[i]>20) return 0; ++t->count[len[i]]; }
  for (i=1;i<=20;++i) {
    left=left*2-(int)t->count[i]; if (left<0) return 0;
    code=(code+(i>1?t->count[i-1]:0))<<1;
    t->first[i]=code; t->index[i]=offset[i]=total; total+=t->count[i];
  }
  for (i=0;i<n;++i) t->symbol[offset[len[i]]++]=i;
  return 1;
}
static int mulib_bzip2_symbol_ (mulib_codec_bits_t *b, const mulib_bzip2_tree_ *t, unsigned *symbol)
{
  unsigned i,code=0; uint32_t v;
  for (i=1;i<=20;++i) {
    if (!mulib_codec_get (b,1,&v)) return 0;
    code=(code<<1)|v;
    if (code>=t->first[i] && code-t->first[i]<t->count[i]) {
      *symbol=t->symbol[t->index[i]+code-t->first[i]]; return 1;
    }
  }
  return 0;
}
static int mulib_bzip2_decode_block_ (mulib_codec_bits_t *b, size_t maximum,
                                     size_t limit, mulib_buffer_t *out, uint32_t *combined)
{
  mulib_bzip2_tree_ trees[6]; unsigned char alphabet[256],mtf[256],*selectors=NULL,*last=NULL;
  uint32_t *tt=NULL,v,stored_crc,origin; unsigned nalpha=0,ng,ns,i,j,lens[258],s,group=0,left=0;
  size_t length=0,run=0,power=1,before=out->length; int rc=MULIB_CODEC_EFORMAT;
  uint32_t counts[257]={0};
#define MULIB_BZ_GET_(bits,dst) do { if (!mulib_codec_get (b,(bits),&v)) goto done; (dst)=v; } while (0)
  MULIB_BZ_GET_(32,stored_crc); MULIB_BZ_GET_(1,v);
  if (v) { rc=MULIB_CODEC_EUNSUPPORTED; goto done; }
  MULIB_BZ_GET_(24,origin); MULIB_BZ_GET_(16,s);
  for (i=0;i<16;++i) if (s&(1u<<(15-i))) for (j=0;j<16;++j) {
    MULIB_BZ_GET_(1,v); if (v) alphabet[nalpha++]=(unsigned char)(i*16+j);
  }
  if (!nalpha) goto done;
  MULIB_BZ_GET_(3,ng); MULIB_BZ_GET_(15,ns);
  if (ng<2 || ng>6 || !ns || ns>18002) goto done;
  selectors=(unsigned char *)malloc (ns); last=(unsigned char *)malloc (maximum);
  if (!selectors || !last) { rc=MULIB_CODEC_ENOMEM; goto done; }
  for (i=0;i<ng;++i) mtf[i]=(unsigned char)i;
  for (i=0;i<ns;++i) {
    unsigned index=0; do { MULIB_BZ_GET_(1,v); if (v && ++index>=ng) goto done; } while (v);
    selectors[i]=mtf[index]; memmove (mtf+1,mtf,index); mtf[0]=selectors[i];
  }
  for (i=0;i<ng;++i) {
    int len; MULIB_BZ_GET_(5,len);
    for (j=0;j<nalpha+2;++j) {
      for (;;) {
        if (len<1 || len>20) goto done;
        MULIB_BZ_GET_(1,v); if (!v) break;
        MULIB_BZ_GET_(1,v); len+=v?-1:1;
      }
      lens[j]=(unsigned)len;
    }
    if (!mulib_bzip2_make_tree_ (&trees[i],lens,nalpha+2)) goto done;
  }
  memcpy (mtf,alphabet,nalpha);
  for (;;) {
    if (!left) { if (group>=ns) goto done; left=50; ++group; }
    --left;
    if (!mulib_bzip2_symbol_ (b,&trees[selectors[group-1]],&s)) goto done;
    if (s<2) {
      size_t add=power*(s+1);
      if (add>maximum-length-run) goto done;
      run+=add;
      if (power>maximum/2) goto done;
      power*=2; continue;
    }
    if (run) { memset (last+length,mtf[0],run); length+=run; run=0; power=1; }
    if (s==nalpha+1) break;
    if (s>nalpha || length==maximum) goto done;
    { unsigned char c=mtf[s-1]; memmove (mtf+1,mtf,s-1); mtf[0]=c; last[length++]=c; }
  }
  if (!length || origin>=length) goto done;
  tt=(uint32_t *)malloc (length*sizeof (*tt)); if (!tt) { rc=MULIB_CODEC_ENOMEM; goto done; }
  for (i=0;i<length;++i) ++counts[last[i]+1];
  for (i=1;i<257;++i) counts[i]+=counts[i-1];
  for (i=0;i<length;++i) tt[counts[last[i]]++]=i;
  { uint32_t pos=tt[origin]; unsigned previous=256,repeats=0;
    size_t k;
    for (k=0;k<length;++k) {
      unsigned char c=last[pos]; pos=tt[pos];
      if (repeats==4) {
        size_t total=c; unsigned char value=(unsigned char)previous;
        if (out->length>limit || total>limit-out->length) { rc=MULIB_CODEC_ELIMIT; goto done; }
        rc=mulib_codec_reserve (out,out->length+total,limit); if (rc) goto done;
        if (total) memset (out->data+out->length,value,total);
        out->length+=total; previous=256; repeats=0;
      } else {
        rc=mulib_codec_append (out,&c,1,limit); if (rc) goto done;
        if (c==previous) ++repeats; else { previous=c; repeats=1; }
      }
    }
    if (repeats==4) { rc=MULIB_CODEC_EFORMAT; goto done; }
  }
  if (mulib_bzip2_crc_ (out->data+before,out->length-before)!=stored_crc) { rc=MULIB_CODEC_ECHECKSUM; goto done; }
  *combined=(*combined<<1|*combined>>31)^stored_crc; rc=0;
#undef MULIB_BZ_GET_
done:
  free (tt); free (selectors); free (last); return rc;
}
int mulib_bzip2_decode (const void *data, size_t size, size_t limit, mulib_buffer_t *out)
{
  const unsigned char *p=(const unsigned char *)data; mulib_buffer_t tmp={0}; size_t at=0;
  int rc=MULIB_CODEC_EFORMAT;
  if ((!data && size) || !mulib_codec_buffer_valid (out)) return MULIB_CODEC_EINVAL;
  if (!size) return rc;
  while (at<size) {
    mulib_codec_bits_t b; uint32_t combined=0,hi,lo,crc; size_t maximum;
    rc=MULIB_CODEC_EFORMAT;
    if (size-at<4 || memcmp (p+at,"BZh",3) || p[at+3]<'1' || p[at+3]>'9') break;
    maximum=(size_t)(p[at+3]-'0')*100000; memset (&b,0,sizeof (b)); b.data=p; b.size=size; b.pos=at+4;
    for (;;) {
      rc=MULIB_CODEC_EFORMAT;
      if (!mulib_codec_get (&b,24,&hi) || !mulib_codec_get (&b,24,&lo)) break;
      if (hi==0x177245 && lo==0x385090) {
        if (!mulib_codec_get (&b,32,&crc)) break;
        if (crc!=combined) { rc=MULIB_CODEC_ECHECKSUM; break; }
        if (b.count && (b.bits&((1u<<b.count)-1))) break;
        at=b.pos; rc=0; break;
      }
      if (hi!=0x314159 || lo!=0x265359) break;
      rc=mulib_bzip2_decode_block_ (&b,maximum,limit,&tmp,&combined); if (rc) break;
    }
    if (rc) break;
  }
  if (!rc && !mulib_buffer_append (out,tmp.data,tmp.length)) rc=MULIB_CODEC_ENOMEM;
  mulib_buffer_free (&tmp); return rc;
}
#endif
