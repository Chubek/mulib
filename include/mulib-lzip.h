/** @file mulib-lzip.h
 * @brief Lzip v1 members with LZMA-302, CRC32 and 64-bit size verification.
 * Define MULIB_IMPL_LZIP, LZMA, CODEC_CORE, BUFFER and CRC32.
 */
#ifndef MULIB_LZIP_H
#define MULIB_LZIP_H
#include "mulib-lzma.h"
#include "mulib-crc32.h"
#ifdef __cplusplus
extern "C" {
#endif
/** @brief Append one deterministic 8 MiB dictionary member; alias-safe. */
int mulib_lzip_encode (const void *data, size_t size, mulib_buffer_t *out);
/** @brief Decode concatenated v1 members within an aggregate output limit.
 * Verify EOS, CRC, data and member sizes; reject trailing garbage atomically. */
int mulib_lzip_decode (const void *data, size_t size, size_t limit, mulib_buffer_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_LZIP) && !defined(MULIB_LZIP_IMPLEMENTED)
#define MULIB_LZIP_IMPLEMENTED
int mulib_lzip_encode (const void *data, size_t size, mulib_buffer_t *out)
{
  const unsigned char h[6]={'L','Z','I','P',1,23}; unsigned char tail[20];
  mulib_buffer_t tmp={0}; int rc;
  if ((!data && size) || !mulib_codec_buffer_valid (out)) return MULIB_CODEC_EINVAL;
  rc=mulib_codec_append (&tmp,h,6,SIZE_MAX);
  if (!rc) rc=mulib_lzma_raw_encode (data,size,&tmp);
  if (!rc && tmp.length>SIZE_MAX-20) rc=MULIB_CODEC_ELIMIT;
  mulib_codec_put32 (tail,mulib_crc32 (data,size)); mulib_codec_put64 (tail+4,size);
  mulib_codec_put64 (tail+12,(uint64_t)tmp.length+20);
  if (!rc) rc=mulib_codec_append (&tmp,tail,20,SIZE_MAX);
  if (!rc && !mulib_buffer_append (out,tmp.data,tmp.length)) rc=MULIB_CODEC_ENOMEM;
  mulib_buffer_free (&tmp); return rc;
}
int mulib_lzip_decode (const void *data, size_t size, size_t limit, mulib_buffer_t *out)
{
  const unsigned char *p=(const unsigned char *)data; mulib_buffer_t tmp={0}; size_t at=0;
  int rc=MULIB_CODEC_EFORMAT;
  if ((!data && size) || !mulib_codec_buffer_valid (out)) return MULIB_CODEC_EINVAL;
  if (!size) return rc;
  while (at<size) {
    size_t start=at,before=tmp.length,used; unsigned log; uint32_t dict;
    rc=MULIB_CODEC_EFORMAT;
    if (size-at<6 || memcmp (p+at,"LZIP",4)) break;
    if (p[at+4]!=1) { rc=MULIB_CODEC_EUNSUPPORTED; break; }
    log=p[at+5]&31;
    if (log<12 || log>29) break;
    dict=(uint32_t)1<<log; dict-=(dict/16)*(p[at+5]>>5);
    if (dict<4096) break;
    at+=6;
    rc=mulib_lzma_raw_decode (p+at,size-at,93,dict,limit-before,&used,&tmp); if (rc) break;
    at+=used; rc=MULIB_CODEC_EFORMAT;
    if (size-at<20 || mulib_codec_le64 (p+at+4)!=tmp.length-before
        || mulib_codec_le64 (p+at+12)!=(uint64_t)(at-start)+20) break;
    if (mulib_codec_le32 (p+at)!=mulib_crc32 (tmp.data?tmp.data+before:NULL,tmp.length-before)) {
      rc=MULIB_CODEC_ECHECKSUM; break;
    }
    at+=20; rc=0;
  }
  if (!rc && !mulib_buffer_append (out,tmp.data,tmp.length)) rc=MULIB_CODEC_ENOMEM;
  mulib_buffer_free (&tmp); return rc;
}
#endif
