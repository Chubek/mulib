/** @file mulib-gzip.h
 * @brief RFC 1952 gzip compression, concatenated members and integrity checks.
 * Define MULIB_IMPL_GZIP, CODEC_CORE, BUFFER, DEFLATE and CRC32.
 */
#ifndef MULIB_GZIP_H
#define MULIB_GZIP_H
#include "mulib-codec-core.h"
#include "mulib-deflate.h"
#include "mulib-crc32.h"
#ifdef __cplusplus
extern "C" {
#endif
/** @brief Append one deterministic member; input may alias output. */
int mulib_gzip_encode (const void *data, size_t size, mulib_buffer_t *out);
/** @brief Decode all members, verify header/data CRCs and sizes. limit bounds
 * the aggregate decoded bytes, excluding out's prefix. Reject trailing garbage.
 * Input may alias output. All failures leave output unchanged. */
int mulib_gzip_decode (const void *data, size_t size, size_t limit, mulib_buffer_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_GZIP) && !defined(MULIB_GZIP_IMPLEMENTED)
#define MULIB_GZIP_IMPLEMENTED
int mulib_gzip_encode (const void *data, size_t size, mulib_buffer_t *out)
{
  static const unsigned char header[10]={31,139,8,0,0,0,0,0,0,255};
  unsigned char tail[8]; mulib_buffer_t tmp={0}; int rc;
  if ((!data && size) || !mulib_codec_buffer_valid (out)) return MULIB_CODEC_EINVAL;
  rc=mulib_codec_append (&tmp,header,10,SIZE_MAX);
  if (!rc) rc=mulib_deflate_encode (data,size,&tmp);
  mulib_codec_put32 (tail,mulib_crc32 (data,size)); mulib_codec_put32 (tail+4,(uint32_t)size);
  if (!rc) rc=mulib_codec_append (&tmp,tail,8,SIZE_MAX);
  if (!rc && !mulib_buffer_append (out,tmp.data,tmp.length)) rc=MULIB_CODEC_ENOMEM;
  mulib_buffer_free (&tmp); return rc;
}
int mulib_gzip_decode (const void *data, size_t size, size_t limit, mulib_buffer_t *out)
{
  const unsigned char *p=(const unsigned char *)data;
  mulib_buffer_t tmp={0}; size_t at=0; int rc=MULIB_CODEC_EFORMAT;
  if ((!data && size) || !mulib_codec_buffer_valid (out)) return MULIB_CODEC_EINVAL;
  if (!size) return rc;
  while (at<size) {
    size_t start=at, before=tmp.length, used; unsigned flags;
    rc=MULIB_CODEC_EFORMAT;
    if (size-at<10 || p[at]!=31 || p[at+1]!=139) goto done;
    if (p[at+2]!=8) { rc=MULIB_CODEC_EUNSUPPORTED; goto done; }
    flags=p[at+3]; if (flags&224) goto done;
    at+=10;
    if (flags&4) {
      size_t n;
      if (size-at<2) goto done;
      n=p[at]|(size_t)p[at+1]<<8; at+=2;
      if (n>size-at) goto done;
      at+=n;
    }
    if (flags&8) { while (at<size && p[at]) ++at; if (at==size) goto done; ++at; }
    if (flags&16) { while (at<size && p[at]) ++at; if (at==size) goto done; ++at; }
    if (flags&2) {
      if (size-at<2) goto done;
      if ((mulib_crc32 (p+start,at-start)&65535u)!=(p[at]|(unsigned)p[at+1]<<8)) {
        rc=MULIB_CODEC_ECHECKSUM; goto done;
      }
      at+=2;
    }
    rc=mulib_deflate_decode_prefix (p+at,size-at,limit-before,&used,&tmp);
    if (rc) goto done;
    at+=used; rc=MULIB_CODEC_EFORMAT;
    if (size-at<8 || mulib_codec_le32 (p+at+4)!=(uint32_t)(tmp.length-before)) goto done;
    if (mulib_codec_le32 (p+at)!=mulib_crc32 (tmp.data ? tmp.data+before : NULL,tmp.length-before)) {
      rc=MULIB_CODEC_ECHECKSUM; goto done;
    }
    at+=8;
  }
  rc=mulib_buffer_append (out,tmp.data,tmp.length)?0:MULIB_CODEC_ENOMEM;
done:
  mulib_buffer_free (&tmp); return rc;
}
#endif
