/** @file mulib-lzop.h
 * @brief Dependency-free lzop container and LZO1X block compression.
 * Define MULIB_IMPL_LZOP, LZO, CODEC_CORE, BUFFER, CRC32 and ADLER32.
 */
#ifndef MULIB_LZOP_H
#define MULIB_LZOP_H
#include "mulib-lzo.h"
#include "mulib-crc32.h"
#include "mulib-adler32.h"
#ifdef __cplusplus
extern "C" {
#endif
/** @brief Append a deterministic lzop v1.03 stream with checked 256 KiB blocks. */
int mulib_lzop_encode (const void *data, size_t size, mulib_buffer_t *out);
/** @brief Decode concatenated lzop streams (LZO1X methods 1, 2, 3), verify all
 * selected header/block checksums and bound aggregate output. Atomic/alias-safe. */
int mulib_lzop_decode (const void *data, size_t size, size_t limit, mulib_buffer_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_LZOP) && !defined(MULIB_LZOP_IMPLEMENTED)
#define MULIB_LZOP_IMPLEMENTED
static const unsigned char mulib_lzop_magic_[9]={137,'L','Z','O',0,13,10,26,10};
int mulib_lzop_encode (const void *data, size_t size, mulib_buffer_t *out)
{
  const unsigned char *p=(const unsigned char *)data; mulib_buffer_t tmp={0},block={0};
  unsigned char h[29]={0x10,0x30,0x20,0xa0,0x09,0x40,1,5,0,0,3,0,0,0,0x81,0xa4,0,0,0,0,0,0,0,0,0,0,0,0,0};
  size_t at=0; int rc;
  if ((!data && size) || !mulib_codec_buffer_valid (out)) return MULIB_CODEC_EINVAL;
  /* Both data and compressed CRCs, CRC header, Unix origin. */
  mulib_codec_putbe32 (h+8,UINT32_C(0x03001300));
  mulib_codec_putbe32 (h+25,mulib_crc32 (h,25));
  rc=mulib_codec_append (&tmp,mulib_lzop_magic_,9,SIZE_MAX);
  if (!rc) rc=mulib_codec_append (&tmp,h,29,SIZE_MAX);
  while (!rc && at<size) {
    size_t n=size-at<262144?size-at:262144,stored; unsigned char bh[16];
    block.length=0; rc=mulib_lzo1x_encode (p+at,n,&block); if (rc) break;
    stored=block.length<n?block.length:n;
    mulib_codec_putbe32 (bh,(uint32_t)n); mulib_codec_putbe32 (bh+4,(uint32_t)stored);
    mulib_codec_putbe32 (bh+8,mulib_crc32 (p+at,n));
    mulib_codec_putbe32 (bh+12,mulib_crc32 (block.data,block.length));
    rc=mulib_codec_append (&tmp,bh,stored<n?16:12,SIZE_MAX);
    if (!rc) rc=mulib_codec_append (&tmp,stored<n?block.data:p+at,stored,SIZE_MAX);
    at+=n;
  }
  if (!rc) { unsigned char end[4]={0}; rc=mulib_codec_append (&tmp,end,4,SIZE_MAX); }
  if (!rc && !mulib_buffer_append (out,tmp.data,tmp.length)) rc=MULIB_CODEC_ENOMEM;
  mulib_buffer_free (&block); mulib_buffer_free (&tmp); return rc;
}
int mulib_lzop_decode (const void *data, size_t size, size_t limit, mulib_buffer_t *out)
{
  const unsigned char *p=(const unsigned char *)data; mulib_buffer_t tmp={0}; size_t at=0;
  int rc=MULIB_CODEC_EFORMAT;
  if ((!data && size) || !mulib_codec_buffer_valid (out)) return MULIB_CODEC_EINVAL;
  if (!size) return rc;
  while (at<size) {
    size_t start,n; unsigned version,method; uint32_t flags,crc;
    rc=MULIB_CODEC_EFORMAT;
    if (size-at<16 || memcmp (p+at,mulib_lzop_magic_,9)) break;
    at+=9; start=at; version=(unsigned)p[at]<<8|p[at+1]; at+=4;
    if (version<0x0940) { rc=MULIB_CODEC_EUNSUPPORTED; break; }
    if (version>=0x0940) { if (size-at<2) break; if (((unsigned)p[at]<<8|p[at+1])>0x1040) { rc=MULIB_CODEC_EUNSUPPORTED; break; } at+=2; }
    if (size-at<2) break;
    method=p[at++]; if (version>=0x0940) { if (p[at]>9) break; ++at; }
    if (method<1 || method>3) { rc=MULIB_CODEC_EUNSUPPORTED; break; }
    if (size-at<4) break;
    flags=mulib_codec_be32 (p+at); at+=4;
    if (flags&UINT32_C(0x000fc000)) { rc=MULIB_CODEC_EUNSUPPORTED; break; }
    if (flags&0xc80) { rc=MULIB_CODEC_EUNSUPPORTED; break; }
    if (size-at<13) break;
    at+=12; n=p[at++];
    if (n>size-at || size-at-n<4) break;
    at+=n; crc=(flags&0x1000)?mulib_crc32 (p+start,at-start):mulib_adler32 (p+start,at-start);
    if (crc!=mulib_codec_be32 (p+at)) { rc=MULIB_CODEC_ECHECKSUM; break; } at+=4;
    if (flags&0x40) {
      if (size-at<4) break;
      start=at; n=mulib_codec_be32 (p+at); at+=4;
      if (n>size-at || size-at-n<4) break;
      at+=n; crc=(flags&0x1000)?mulib_crc32 (p+start,n+4):mulib_adler32 (p+start,n+4);
      if (crc!=mulib_codec_be32 (p+at)) { rc=MULIB_CODEC_ECHECKSUM; break; } at+=4;
    }
    for (;;) {
      size_t unpack,pack,before=tmp.length; uint32_t ad=0,cd=0,ac=0,cc=0;
      rc=MULIB_CODEC_EFORMAT;
      if (size-at<4) break;
      unpack=mulib_codec_be32 (p+at); at+=4;
      if (!unpack) { rc=0; break; }
      if (size-at<4) break;
      pack=mulib_codec_be32 (p+at); at+=4;
      if (!pack || pack>unpack) break;
      if (unpack>limit-tmp.length) { rc=MULIB_CODEC_ELIMIT; break; }
#define MULIB_LZOP_CHECK_(flag,value) do { if (flags&(flag)) { if (size-at<4) goto done; (value)=mulib_codec_be32 (p+at); at+=4; } } while (0)
      MULIB_LZOP_CHECK_(1,ad); MULIB_LZOP_CHECK_(0x100,cd);
      if (pack<unpack) { MULIB_LZOP_CHECK_(2,ac); MULIB_LZOP_CHECK_(0x200,cc); }
#undef MULIB_LZOP_CHECK_
      if (pack>size-at) break;
      if (pack<unpack && (((flags&2) && mulib_adler32 (p+at,pack)!=ac)
                         || ((flags&0x200) && mulib_crc32 (p+at,pack)!=cc))) { rc=MULIB_CODEC_ECHECKSUM; break; }
      rc=pack==unpack?mulib_codec_append (&tmp,p+at,pack,limit):mulib_lzo1x_decode (p+at,pack,unpack,&tmp);
      if (rc) break;
      if (tmp.length-before!=unpack) { rc=MULIB_CODEC_EFORMAT; break; }
      if (((flags&1) && mulib_adler32 (tmp.data+before,unpack)!=ad)
          || ((flags&0x100) && mulib_crc32 (tmp.data+before,unpack)!=cd)) { rc=MULIB_CODEC_ECHECKSUM; break; }
      at+=pack;
    }
    if (rc) break;
  }
  if (!rc && !mulib_buffer_append (out,tmp.data,tmp.length)) rc=MULIB_CODEC_ENOMEM;
done:
  mulib_buffer_free (&tmp); return rc;
}
#endif
