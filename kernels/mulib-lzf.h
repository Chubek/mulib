/** @file mulib-lzf.h
 * @brief Dependency-free LZF blocks and standard ZV-framed .lzf streams.
 * Define MULIB_IMPL_LZF, CODEC_CORE and BUFFER.
 */
#ifndef MULIB_LZF_H
#define MULIB_LZF_H
#include "mulib-codec-core.h"
#ifdef __cplusplus
extern "C" {
#endif
/** @brief Compress a raw LZF block; append atomically, including empty input. */
int mulib_lzf_raw_encode (const void *data, size_t size, mulib_buffer_t *out);
/** @brief Decode a complete raw block within limit; input may alias output. */
int mulib_lzf_raw_decode (const void *data, size_t size, size_t limit, mulib_buffer_t *out);
/** @brief Encode ZV chunks, using compressed chunks only when smaller. */
int mulib_lzf_encode (const void *data, size_t size, mulib_buffer_t *out);
/** @brief Decode all ZV chunks and validate their declared lengths. */
int mulib_lzf_decode (const void *data, size_t size, size_t limit, mulib_buffer_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_LZF) && !defined(MULIB_LZF_IMPLEMENTED)
#define MULIB_LZF_IMPLEMENTED
int mulib_lzf_raw_encode (const void *data, size_t size, mulib_buffer_t *out)
{
  const unsigned char *p=(const unsigned char *)data;
  mulib_buffer_t tmp={0}; size_t *last, at=0, lit=0, i; int rc=0;
  if ((!data && size) || !mulib_codec_buffer_valid (out)) return MULIB_CODEC_EINVAL;
  last=(size_t *)malloc (8192*sizeof (*last)); if (!last) return MULIB_CODEC_ENOMEM;
  for (i=0;i<8192;++i) last[i]=SIZE_MAX;
  while (at<size) {
    size_t len=0, prev=0, distance=0;
    if (size-at>=3) {
      unsigned hash=((unsigned)p[at]*251u+(unsigned)p[at+1]*31u+p[at+2])&8191u;
      prev=last[hash]; last[hash]=at;
      if (prev!=SIZE_MAX && at-prev<=8192) {
        while (len<264 && len<size-at && p[prev+len]==p[at+len]) ++len;
        distance=at-prev-1;
      }
    }
    if (len<3) { ++at; if (at-lit<32 && at<size) continue; }
    if (at>lit) {
      unsigned char c=(unsigned char)(at-lit-1);
      rc=mulib_codec_append (&tmp,&c,1,SIZE_MAX);
      if (!rc) rc=mulib_codec_append (&tmp,p+lit,at-lit,SIZE_MAX);
      if (rc) break;
    }
    if (len>=3) {
      unsigned char h[3]; size_t n=2;
      h[0]=(unsigned char)(((len-2<7?len-2:7)<<5)|(distance>>8));
      if (len>=9) { h[1]=(unsigned char)(len-9); h[2]=(unsigned char)distance; n=3; }
      else h[1]=(unsigned char)distance;
      rc=mulib_codec_append (&tmp,h,n,SIZE_MAX); if (rc) break;
      at+=len;
    }
    lit=at;
  }
  if (!rc && !mulib_buffer_append (out,tmp.data,tmp.length)) rc=MULIB_CODEC_ENOMEM;
  free (last); mulib_buffer_free (&tmp); return rc;
}
int mulib_lzf_raw_decode (const void *data, size_t size, size_t limit, mulib_buffer_t *out)
{
  const unsigned char *p=(const unsigned char *)data;
  mulib_buffer_t tmp={0}; size_t at=0; int rc=0;
  if ((!data && size) || !mulib_codec_buffer_valid (out)) return MULIB_CODEC_EINVAL;
  while (at<size) {
    unsigned c=p[at++]; size_t len, distance;
    if (c<32) {
      len=c+1; if (len>size-at) { rc=MULIB_CODEC_EFORMAT; break; }
      rc=mulib_codec_append (&tmp,p+at,len,limit); at+=len;
    } else {
      len=c>>5; distance=(size_t)(c&31)<<8;
      if (len==7) { if (at==size) { rc=MULIB_CODEC_EFORMAT; break; } len+=p[at++]; }
      if (at==size) { rc=MULIB_CODEC_EFORMAT; break; }
      distance+=p[at++]+1; rc=mulib_codec_match (&tmp,distance,len+2,limit);
    }
    if (rc) break;
  }
  if (!rc && !mulib_buffer_append (out,tmp.data,tmp.length)) rc=MULIB_CODEC_ENOMEM;
  mulib_buffer_free (&tmp); return rc;
}
int mulib_lzf_encode (const void *data, size_t size, mulib_buffer_t *out)
{
  const unsigned char *p=(const unsigned char *)data;
  mulib_buffer_t tmp={0}, block={0}; size_t at=0; int rc=0;
  if ((!data && size) || !mulib_codec_buffer_valid (out)) return MULIB_CODEC_EINVAL;
  do {
    size_t n=size-at<65535?size-at:65535, stored; unsigned char h[7]={'Z','V',0,0,0,0,0};
    block.length=0; rc=mulib_lzf_raw_encode (p?p+at:NULL,n,&block); if (rc) break;
    h[2]=(unsigned char)(block.length<n); stored=h[2]?block.length:n;
    h[3]=(unsigned char)(stored>>8); h[4]=(unsigned char)stored;
    h[5]=(unsigned char)(n>>8); h[6]=(unsigned char)n;
    rc=mulib_codec_append (&tmp,h,h[2]?7:5,SIZE_MAX);
    if (!rc) rc=mulib_codec_append (&tmp,h[2]?block.data:(p?p+at:NULL),stored,SIZE_MAX);
    at+=n;
  } while (!rc && at<size);
  if (!rc && !mulib_buffer_append (out,tmp.data,tmp.length)) rc=MULIB_CODEC_ENOMEM;
  mulib_buffer_free (&block); mulib_buffer_free (&tmp); return rc;
}
int mulib_lzf_decode (const void *data, size_t size, size_t limit, mulib_buffer_t *out)
{
  const unsigned char *p=(const unsigned char *)data;
  mulib_buffer_t tmp={0}; size_t at=0; int rc=MULIB_CODEC_EFORMAT;
  if ((!data && size) || !mulib_codec_buffer_valid (out)) return MULIB_CODEC_EINVAL;
  if (!size) return rc;
  while (at<size) {
    size_t stored,n,before=tmp.length; unsigned type;
    rc=MULIB_CODEC_EFORMAT;
    if (size-at<5 || p[at]!='Z' || p[at+1]!='V') break;
    type=p[at+2]; stored=(size_t)p[at+3]<<8|p[at+4]; at+=5; n=stored;
    if (type>1) { rc=MULIB_CODEC_EUNSUPPORTED; break; }
    if (type) { if (size-at<2) break; n=(size_t)p[at]<<8|p[at+1]; at+=2; }
    if (stored>size-at) break;
    if (n>limit-tmp.length) { rc=MULIB_CODEC_ELIMIT; break; }
    rc=type?mulib_lzf_raw_decode (p+at,stored,n,&tmp):mulib_codec_append (&tmp,p+at,n,limit);
    if (rc) break;
    if (tmp.length-before!=n) { rc=MULIB_CODEC_EFORMAT; break; }
    at+=stored;
  }
  if (!rc && !mulib_buffer_append (out,tmp.data,tmp.length)) rc=MULIB_CODEC_ENOMEM;
  mulib_buffer_free (&tmp); return rc;
}
#endif
