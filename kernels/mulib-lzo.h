/** @file mulib-lzo.h
 * @brief Dependency-free LZO1X bytecode compressor/decompressor shared by lzop.
 * Define MULIB_IMPL_LZO, CODEC_CORE and BUFFER. Original (v0) LZO1X format.
 */
#ifndef MULIB_LZO_H
#define MULIB_LZO_H
#include "mulib-codec-core.h"
#ifdef __cplusplus
extern "C" {
#endif
/** @brief Append a raw LZO1X stream with bounded greedy LZ matches and EOS. */
int mulib_lzo1x_encode (const void *data, size_t size, mulib_buffer_t *out);
/** @brief Decode exactly one raw LZO1X stream with checked references and EOS.
 * Aggregate output is bounded by limit; failures preserve out, aliasing allowed. */
int mulib_lzo1x_decode (const void *data, size_t size, size_t limit, mulib_buffer_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_LZO) && !defined(MULIB_LZO_IMPLEMENTED)
#define MULIB_LZO_IMPLEMENTED
static int mulib_lzo_length_ (const unsigned char *p, size_t n, size_t *at, size_t base, size_t *length)
{
  size_t len=base;
  while (*at<n && !p[*at]) { if (len>SIZE_MAX-255) return 0; len+=255; ++*at; }
  if (*at==n || p[*at]>SIZE_MAX-len) return 0;
  *length=len+p[(*at)++]; return 1;
}
int mulib_lzo1x_decode (const void *data, size_t size, size_t limit, mulib_buffer_t *out)
{
  const unsigned char *p=(const unsigned char *)data; mulib_buffer_t tmp={0};
  size_t at=0; unsigned state=0; int ended=0,rc=MULIB_CODEC_EFORMAT;
  if ((!data && size) || !mulib_codec_buffer_valid (out)) return MULIB_CODEC_EINVAL;
  if (!size) return rc;
  if (p[0]>17) {
    size_t len=p[at++]-17; if (len>size-at) goto done;
    rc=mulib_codec_append (&tmp,p+at,len,limit); if (rc) goto done;
    at+=len; state=len<4?(unsigned)len:4;
  }
  while (at<size) {
    unsigned op=p[at++],trail; size_t len,distance; rc=MULIB_CODEC_EFORMAT;
    if (op<16 && !state) {
      len=op;
      if (!len && !mulib_lzo_length_ (p,size,&at,15,&len)) goto done;
      if (len>SIZE_MAX-3) goto done;
      len+=3; if (len>size-at) goto done;
      rc=mulib_codec_append (&tmp,p+at,len,limit); if (rc) goto done;
      at+=len; state=4; continue;
    }
    if (op<16) {
      if (at==size) goto done;
      len=state==4?3:2; distance=(op>>2)+(size_t)p[at++]*4+(state==4?2049:1); trail=op&3;
    } else if (op>=64) {
      if (at==size) goto done;
      len=(op>>5)+1; distance=((op>>2)&7)+(size_t)p[at++]*8+1; trail=op&3;
    } else {
      unsigned operand; len=op&(op>=32?31:7);
      if (!len && !mulib_lzo_length_ (p,size,&at,op>=32?31:7,&len)) goto done;
      if (len>SIZE_MAX-2 || size-at<2) goto done;
      len+=2; operand=p[at]|(unsigned)p[at+1]<<8; at+=2; trail=operand&3;
      if (op>=32) distance=(operand>>2)+1;
      else {
        distance=(size_t)(op&8)*2048+(operand>>2);
        if (!distance) {
          if (op!=17 || operand || at!=size) goto done;
          ended=1; rc=0; break;
        }
        distance+=16384;
      }
    }
    rc=mulib_codec_match (&tmp,distance,len,limit); if (rc) goto done;
    if (trail>size-at) { rc=MULIB_CODEC_EFORMAT; goto done; }
    rc=mulib_codec_append (&tmp,p+at,trail,limit); if (rc) goto done;
    at+=trail; state=trail;
  }
  if (!ended) rc=MULIB_CODEC_EFORMAT;
  if (!rc && !mulib_buffer_append (out,tmp.data,tmp.length)) rc=MULIB_CODEC_ENOMEM;
done:
  mulib_buffer_free (&tmp); return rc;
}
static int mulib_lzo_extend_ (mulib_buffer_t *b, size_t n)
{
  unsigned char c=0; int rc;
  while (n>255) { rc=mulib_codec_append (b,&c,1,SIZE_MAX); if (rc) return rc; n-=255; }
  c=(unsigned char)n; return mulib_codec_append (b,&c,1,SIZE_MAX);
}
static int mulib_lzo_literals_ (mulib_buffer_t *b, const unsigned char *p, size_t n, int first)
{
  unsigned char c; int rc;
  if (!n) return 0;
  if (first && n<=238) { c=(unsigned char)(n+17); rc=mulib_codec_append (b,&c,1,SIZE_MAX); }
  else if (n<=18) { c=(unsigned char)(n-3); rc=mulib_codec_append (b,&c,1,SIZE_MAX); }
  else { c=0; rc=mulib_codec_append (b,&c,1,SIZE_MAX); if (!rc) rc=mulib_lzo_extend_ (b,n-18); }
  return rc?rc:mulib_codec_append (b,p,n,SIZE_MAX);
}
typedef struct mulib_lzo_match_ { size_t at,len,distance; } mulib_lzo_match_;
int mulib_lzo1x_encode (const void *data, size_t size, mulib_buffer_t *out)
{
  const unsigned char *p=(const unsigned char *)data; mulib_buffer_t tmp={0},matches={0};
  size_t *last,at=0,i,count; int rc=0;
  if ((!data && size) || !mulib_codec_buffer_valid (out)) return MULIB_CODEC_EINVAL;
  last=(size_t *)malloc (8192*sizeof (*last)); if (!last) return MULIB_CODEC_ENOMEM;
  for (i=0;i<8192;++i) last[i]=SIZE_MAX;
  while (size-at>=3) {
    unsigned hash=((unsigned)p[at]*251u+(unsigned)p[at+1]*31u+p[at+2])&8191u;
    size_t prev=last[hash],len=0; last[hash]=at;
    if (prev!=SIZE_MAX && at-prev<=16384) while (len<size-at && len<65536 && p[prev+len]==p[at+len]) ++len;
    if (len<3) { ++at; continue; }
    { mulib_lzo_match_ m; m.at=at; m.len=len; m.distance=at-prev;
      rc=mulib_codec_append (&matches,&m,sizeof (m),SIZE_MAX); if (rc) goto done; }
    at+=len;
  }
  count=matches.length/sizeof (mulib_lzo_match_);
  if (!count) rc=mulib_lzo_literals_ (&tmp,p,size,1);
  else {
    mulib_lzo_match_ m; memcpy (&m,matches.data,sizeof (m));
    rc=mulib_lzo_literals_ (&tmp,p,m.at,1);
    for (i=0;!rc && i<count;++i) {
      size_t next=size,gap; unsigned char h[3]; unsigned operand;
      memcpy (&m,matches.data+i*sizeof (m),sizeof (m));
      if (i+1<count) { mulib_lzo_match_ following; memcpy (&following,matches.data+(i+1)*sizeof (m),sizeof (m)); next=following.at; }
      gap=next-m.at-m.len; operand=(unsigned)((m.distance-1)<<2)|(gap<=3?(unsigned)gap:0);
      h[0]=(unsigned char)(32+(m.len<=33?m.len-2:0));
      rc=mulib_codec_append (&tmp,h,1,SIZE_MAX);
      if (!rc && m.len>33) rc=mulib_lzo_extend_ (&tmp,m.len-33);
      h[0]=(unsigned char)operand; h[1]=(unsigned char)(operand>>8);
      if (!rc) rc=mulib_codec_append (&tmp,h,2,SIZE_MAX);
      if (!rc) rc=gap<=3?mulib_codec_append (&tmp,p+m.at+m.len,gap,SIZE_MAX):mulib_lzo_literals_ (&tmp,p+m.at+m.len,gap,0);
    }
  }
  if (!rc) { const unsigned char eos[3]={17,0,0}; rc=mulib_codec_append (&tmp,eos,3,SIZE_MAX); }
  if (!rc && !mulib_buffer_append (out,tmp.data,tmp.length)) rc=MULIB_CODEC_ENOMEM;
done:
  free (last); mulib_buffer_free (&matches); mulib_buffer_free (&tmp); return rc;
}
#endif
