/** @file mulib-xz.h
 * @brief Dependency-free XZ streams with LZMA2, block/index validation and checks.
 * Define MULIB_IMPL_XZ, LZMA, CODEC_CORE, BUFFER, CRC32, AUTHCRYPTO and BCJ.
 */
#ifndef MULIB_XZ_H
#define MULIB_XZ_H
#include "mulib-lzma.h"
#include "mulib-crc32.h"
#include "mulib-authcrypto.h"
#include "mulib-bcj.h"
#ifdef __cplusplus
extern "C" {
#endif
/** @brief Append a deterministic CRC64-protected LZMA2 XZ stream. */
int mulib_xz_encode (const void *data, size_t size, mulib_buffer_t *out);
/** @brief Decode concatenated XZ streams, validate headers, blocks, index,
 * footer, padding and None/CRC32/CRC64/SHA256 checks. Supports LZMA2 and Delta
 * and classic BCJ filter chains. Unsupported filters are diagnosed, not ignored. */
int mulib_xz_decode (const void *data, size_t size, size_t limit, mulib_buffer_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_XZ) && !defined(MULIB_XZ_IMPLEMENTED)
#define MULIB_XZ_IMPLEMENTED
static int mulib_xz_vli_ (const unsigned char *p, size_t end, size_t *at, uint64_t *value)
{
  uint64_t v=0; unsigned i;
  for (i=0;i<9;++i) {
    unsigned c; if (*at==end) return 0; c=p[(*at)++];
    if (i && !c) return 0;
    v|=(uint64_t)(c&127)<<(i*7);
    if (!(c&128)) { *value=v; return 1; }
  }
  return 0;
}
static int mulib_xz_put_vli_ (mulib_buffer_t *b, uint64_t n)
{
  unsigned char p[9]; size_t at=0;
  if (n>INT64_MAX) return MULIB_CODEC_ELIMIT;
  do { p[at++]=(unsigned char)((n&127)|(n>=128?128:0)); n>>=7; } while (n);
  return mulib_codec_append (b,p,at,SIZE_MAX);
}
int mulib_xz_encode (const void *data, size_t size, mulib_buffer_t *out)
{
  unsigned char h[12]={253,'7','z','X','Z',0,0,4,0,0,0,0},bh[12]={2,0,0x21,1,22,0,0,0,0,0,0,0};
  unsigned char check[8],footer[12]={0},zero=0; mulib_buffer_t tmp={0},index={0}; size_t start,packed; int rc;
  if ((!data && size) || !mulib_codec_buffer_valid (out)) return MULIB_CODEC_EINVAL;
  mulib_codec_put32 (h+8,mulib_crc32 (h+6,2)); mulib_codec_put32 (bh+8,mulib_crc32 (bh,8));
  rc=mulib_codec_append (&tmp,h,12,SIZE_MAX);
  if (!rc) rc=mulib_codec_append (&tmp,bh,12,SIZE_MAX);
  start=tmp.length;
  if (!rc) rc=mulib_lzma2_encode (data,size,&tmp);
  packed=tmp.length-start;
  while (!rc && (tmp.length&3)) rc=mulib_codec_append (&tmp,&zero,1,SIZE_MAX);
  mulib_codec_put64 (check,mulib_codec_crc64 (data,size));
  if (!rc) rc=mulib_codec_append (&tmp,check,8,SIZE_MAX);
  if (!rc) { const unsigned char ih[2]={0,1}; rc=mulib_codec_append (&index,ih,2,SIZE_MAX); }
  if (!rc) rc=mulib_xz_put_vli_ (&index,(uint64_t)packed+20);
  if (!rc) rc=mulib_xz_put_vli_ (&index,size);
  while (!rc && (index.length&3)) rc=mulib_codec_append (&index,&zero,1,SIZE_MAX);
  mulib_codec_put32 (check,mulib_crc32 (index.data,index.length));
  if (!rc) rc=mulib_codec_append (&index,check,4,SIZE_MAX);
  if (!rc && index.length/4-1>UINT32_MAX) rc=MULIB_CODEC_ELIMIT;
  mulib_codec_put32 (footer+4,(uint32_t)(index.length/4-1)); footer[9]=4; footer[10]='Y'; footer[11]='Z';
  mulib_codec_put32 (footer,mulib_crc32 (footer+4,6));
  if (!rc) rc=mulib_codec_append (&tmp,index.data,index.length,SIZE_MAX);
  if (!rc) rc=mulib_codec_append (&tmp,footer,12,SIZE_MAX);
  if (!rc && !mulib_buffer_append (out,tmp.data,tmp.length)) rc=MULIB_CODEC_ENOMEM;
  mulib_buffer_free (&index); mulib_buffer_free (&tmp); return rc;
}
int mulib_xz_decode (const void *data, size_t size, size_t limit, mulib_buffer_t *out)
{
  const unsigned char *p=(const unsigned char *)data; mulib_buffer_t tmp={0},records={0}; size_t at=0;
  int rc=MULIB_CODEC_EFORMAT;
  if ((!data && size) || !mulib_codec_buffer_valid (out)) return MULIB_CODEC_EINVAL;
  if (!size || (size&3)) return rc;
  while (at<size) {
    unsigned check_id,check_size; size_t count=0,index_start,record_start; uint64_t nrec;
    records.length=0; rc=MULIB_CODEC_EFORMAT;
    if (size-at<12 || memcmp (p+at,"\xfd" "7zXZ\0",6)) goto done;
    if (mulib_crc32 (p+at+6,2)!=mulib_codec_le32 (p+at+8)) { rc=MULIB_CODEC_ECHECKSUM; goto done; }
    check_id=p[at+7];
    if (p[at+6] || (check_id&0xf0)) goto done;
    if (check_id!=0 && check_id!=1 && check_id!=4 && check_id!=10) { rc=MULIB_CODEC_EUNSUPPORTED; goto done; }
    check_size=check_id==0?0:check_id==1?4:check_id==4?8:32; at+=12;
    while (at<size && p[at]) {
      size_t block_start=at,hlen=((size_t)p[at]+1)*4,end,pos,used,before=tmp.length,i;
      uint64_t declared_c=UINT64_MAX,declared_u=UINT64_MAX;
      unsigned flags,nfilters,filter[3],nf=0; uint32_t property[3],dict=0;
      rc=MULIB_CODEC_EFORMAT;
      if (hlen>size-at || hlen<8) goto done;
      end=at+hlen-4;
      if (mulib_crc32 (p+at,hlen-4)!=mulib_codec_le32 (p+end)) { rc=MULIB_CODEC_ECHECKSUM; goto done; }
      flags=p[at+1]; if (flags&0x3c) goto done;
      nfilters=(flags&3)+1; pos=at+2;
      if ((flags&0x40) && (!mulib_xz_vli_ (p,end,&pos,&declared_c) || !declared_c)) goto done;
      if ((flags&0x80) && !mulib_xz_vli_ (p,end,&pos,&declared_u)) goto done;
      if (declared_u!=UINT64_MAX && declared_u>limit-tmp.length) { rc=MULIB_CODEC_ELIMIT; goto done; }
      for (i=0;i<nfilters;++i) {
        uint64_t id,n;
        if (!mulib_xz_vli_ (p,end,&pos,&id) || !mulib_xz_vli_ (p,end,&pos,&n) || n>end-pos) goto done;
        if (i+1==nfilters) {
          unsigned prop;
          if (id!=0x21) { rc=MULIB_CODEC_EUNSUPPORTED; goto done; }
          if (n!=1 || p[pos]>40) goto done;
          prop=p[pos]; dict=prop==40?UINT32_MAX:(uint32_t)(2|(prop&1))<<(prop/2+11);
        } else {
          if (id==3) {
            if (n!=1) goto done;
            filter[nf]=3; property[nf++]=p[pos]+1;
          } else if (id>=4 && id<=9) {
            unsigned alignment=id==4?1:id==6?16:id==8?2:4;
            if (n!=0 && n!=4) goto done;
            filter[nf]=(unsigned)id; property[nf]=n?mulib_codec_le32 (p+pos):0;
            if (property[nf]%alignment) goto done;
            ++nf;
          } else { rc=MULIB_CODEC_EUNSUPPORTED; goto done; }
        }
        pos+=(size_t)n;
      }
      while (pos<end) if (p[pos++]) goto done;
      at=block_start+hlen;
      rc=mulib_lzma2_decode (p+at,size-at,dict,limit-tmp.length,&used,&tmp); if (rc) goto done;
      rc=MULIB_CODEC_EFORMAT;
      if ((declared_c!=UINT64_MAX && declared_c!=used)
          || (declared_u!=UINT64_MAX && declared_u!=tmp.length-before)) goto done;
      at+=used;
      while ((at-block_start)&3) { if (at==size || p[at++]) goto done; }
      if (check_size>size-at) goto done;
      while (nf) {
        --nf;
        if (filter[nf]==3) {
          unsigned distance=property[nf];
          for (i=distance;i<tmp.length-before;++i) tmp.data[before+i]=(unsigned char)(tmp.data[before+i]+tmp.data[before+i-distance]);
        } else {
          rc=mulib_bcj_transform (filter[nf],tmp.data?tmp.data+before:NULL,tmp.length-before,property[nf],0);
          if (rc) goto done;
          rc=MULIB_CODEC_EFORMAT;
        }
      }
      if (check_id==1 && mulib_crc32 (tmp.data?tmp.data+before:NULL,tmp.length-before)!=mulib_codec_le32 (p+at)) { rc=MULIB_CODEC_ECHECKSUM; goto done; }
      if (check_id==4 && mulib_codec_crc64 (tmp.data?tmp.data+before:NULL,tmp.length-before)!=mulib_codec_le64 (p+at)) { rc=MULIB_CODEC_ECHECKSUM; goto done; }
      if (check_id==10) {
        unsigned char hash[32];
        if (!mulib_authcrypto_hash (MULIB_AUTHCRYPTO_SHA256,tmp.data?tmp.data+before:NULL,tmp.length-before,hash,32)
            || memcmp (hash,p+at,32)) { rc=MULIB_CODEC_ECHECKSUM; goto done; }
      }
      at+=check_size;
      rc=mulib_xz_put_vli_ (&records,(uint64_t)hlen+used+check_size); if (rc) goto done;
      rc=mulib_xz_put_vli_ (&records,tmp.length-before); if (rc) goto done;
      ++count;
    }
    rc=MULIB_CODEC_EFORMAT;
    if (at==size) goto done;
    index_start=at++;
    if (!mulib_xz_vli_ (p,size,&at,&nrec) || nrec!=count) goto done;
    record_start=at;
    if (records.length>size-at || (records.length && memcmp (records.data,p+at,records.length))) goto done;
    at=record_start+records.length;
    while ((at-index_start)&3) { if (at==size || p[at++]) goto done; }
    if (size-at<16) goto done;
    if (mulib_crc32 (p+index_start,at-index_start)!=mulib_codec_le32 (p+at)) { rc=MULIB_CODEC_ECHECKSUM; goto done; }
    at+=4;
    if ((uint64_t)mulib_codec_le32 (p+at+4)+1!=(at-index_start)/4 || p[at+8] || p[at+9]!=check_id
        || p[at+10]!='Y' || p[at+11]!='Z') goto done;
    if (mulib_crc32 (p+at+4,6)!=mulib_codec_le32 (p+at)) { rc=MULIB_CODEC_ECHECKSUM; goto done; }
    at+=12;
    { size_t pad=at; while (at<size && !p[at]) ++at; if ((at-pad)&3) goto done; }
  }
  rc=mulib_buffer_append (out,tmp.data,tmp.length)?0:MULIB_CODEC_ENOMEM;
done:
  mulib_buffer_free (&records); mulib_buffer_free (&tmp); return rc;
}
#endif
