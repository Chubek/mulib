/** @file mulib-miniwav.h
 * @brief RIFF/WAVE PCM and IEEE float memory encoder/decoder.
 */
#ifndef MULIB_MINIWAV_H
#define MULIB_MINIWAV_H
#include "mulib-audio.h"
#ifdef __cplusplus
extern "C" {
#endif
/** Decode little-endian RIFF PCM8/16/24/32 or IEEE float32/64, including
 * WAVE_FORMAT_EXTENSIBLE with matching valid bits. Output must be empty.
 * Unknown chunks are skipped; RF64, RIFX and compressed WAV are unsupported.
 * Failures leave output unchanged. SIMD accelerates integer normalization. */
mulib_audio_status_t mulib_miniwav_decode (const void *, size_t,
    const mulib_audio_options_t *, mulib_audio_pcm_t *);
/** Encode PCM8/16/24/32 or float32 (floating=1, bits=32).
 * Integer samples are rounded to nearest, clipped to [-1,1). Output must be
 * empty; failure leaves it unchanged. Empty PCM produces a valid empty WAV. */
mulib_audio_status_t mulib_miniwav_encode (const mulib_audio_pcm_t *, unsigned bits,
    int floating, const mulib_audio_options_t *, mulib_audio_bytes_t *);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_MINIWAV) && !defined(MULIB_MINIWAV_IMPLEMENTED)
#define MULIB_MINIWAV_IMPLEMENTED
#include <string.h>
#include <math.h>
#include <float.h>
static uint32_t mulib_miniwav_u_ (const unsigned char *p, unsigned n)
{ uint32_t x=0; unsigned i; for(i=0;i<n;++i) x|=(uint32_t)p[i]<<(8*i); return x; }
static void mulib_miniwav_put_ (unsigned char *p, uint32_t v, unsigned n)
{ unsigned i; for(i=0;i<n;++i) p[i]=(unsigned char)(v>>(8*i)); }
mulib_audio_status_t mulib_miniwav_decode (const void *data, size_t size,
    const mulib_audio_options_t *opt, mulib_audio_pcm_t *out)
{
  const unsigned char *p=(const unsigned char *)data, *fmt=NULL, *samples=NULL;
  size_t at=12, end, fs=0, ns=0, frames, i; unsigned tag, channels, rate, bits, width, align;
  mulib_audio_pcm_t pcm={0}; mulib_audio_status_t st;
  mulib_xsimd_backend_t simd=opt ? opt->simd : MULIB_XSIMD_AUTO;
  if (!out || out->samples || out->frames || (!data && size)) return MULIB_AUDIO_INVALID;
  if (!mulib_xsimd_available(simd)) return MULIB_AUDIO_UNSUPPORTED;
  if (size<12) return MULIB_AUDIO_FORMAT;
  if (!memcmp(p,"RF64",4) || !memcmp(p,"RIFX",4)) return MULIB_AUDIO_UNSUPPORTED;
  if (memcmp(p,"RIFF",4) || memcmp(p+8,"WAVE",4)) return MULIB_AUDIO_FORMAT;
  if (mulib_miniwav_u_(p+4,4)<4 || mulib_miniwav_u_(p+4,4)>size-8) return MULIB_AUDIO_FORMAT;
  end=(size_t)mulib_miniwav_u_(p+4,4)+8;
  while (at<end) {
    size_t n;
    if (end-at<8) return MULIB_AUDIO_FORMAT;
    n=mulib_miniwav_u_(p+at+4,4); at+=8;
    if (n>end-at || (n&1)>end-at-n) return MULIB_AUDIO_FORMAT;
    if (!memcmp(p+at-8,"fmt ",4)) { if (fmt) return MULIB_AUDIO_FORMAT; fmt=p+at; fs=n; }
    else if (!memcmp(p+at-8,"data",4)) { if (samples) return MULIB_AUDIO_UNSUPPORTED; samples=p+at; ns=n; }
    at+=n+(n&1);
  }
  if (!fmt || fs<16 || !samples) return MULIB_AUDIO_FORMAT;
  tag=mulib_miniwav_u_(fmt,2); channels=mulib_miniwav_u_(fmt+2,2);
  rate=mulib_miniwav_u_(fmt+4,4); align=mulib_miniwav_u_(fmt+12,2); bits=mulib_miniwav_u_(fmt+14,2);
  if (tag==65534) {
    static const unsigned char tail[12]={0,0,16,0,128,0,0,170,0,56,155,113};
    if (fs<40 || mulib_miniwav_u_(fmt+16,2)<22 || mulib_miniwav_u_(fmt+16,2)>fs-18) return MULIB_AUDIO_FORMAT;
    if (memcmp(fmt+28,tail,12) || mulib_miniwav_u_(fmt+18,2)!=bits) return MULIB_AUDIO_UNSUPPORTED;
    tag=mulib_miniwav_u_(fmt+24,4);
  }
  if ((tag!=1 && tag!=3) || (tag==1 && bits!=8 && bits!=16 && bits!=24 && bits!=32)
      || (tag==3 && bits!=32 && bits!=64)) return MULIB_AUDIO_UNSUPPORTED;
  if (FLT_RADIX!=2 || sizeof(float)!=4 || sizeof(double)!=8) return MULIB_AUDIO_UNSUPPORTED;
  width=bits/8;
  if (!channels || channels>8 || !rate || rate>768000 || align!=width*channels
      || mulib_miniwav_u_(fmt+8,4)!=(uint64_t)rate*align || ns%align) return MULIB_AUDIO_FORMAT;
  frames=ns/align;
  if (frames>(opt && opt->max_frames ? opt->max_frames : 16777216)) return MULIB_AUDIO_LIMIT;
  st=mulib_audio_pcm_create(&pcm,frames,rate,channels); if (st) return st;
  for (i=0;i<frames*channels;++i) {
    const unsigned char *s=samples+i*width;
    if (tag==3) {
      if (bits==32) { uint32_t u=mulib_miniwav_u_(s,4); memcpy(pcm.samples+i,&u,4); }
      else { uint64_t u=mulib_miniwav_u_(s,4)|((uint64_t)mulib_miniwav_u_(s+4,4)<<32); double v; memcpy(&v,&u,8);
        if(!isfinite(v) || v>FLT_MAX || v<-FLT_MAX) { mulib_audio_pcm_free(&pcm); return MULIB_AUDIO_FORMAT; }
        pcm.samples[i]=(float)v; }
      if (!isfinite(pcm.samples[i])) { mulib_audio_pcm_free(&pcm); return MULIB_AUDIO_FORMAT; }
    } else {
      uint32_t u=mulib_miniwav_u_(s,width);
      int64_t v=bits==8 ? (int64_t)u-128 : (u & (UINT32_C(1)<<(bits-1))) ? (int64_t)u-(INT64_C(1)<<bits) : (int64_t)u;
      pcm.samples[i]=(float)v;
    }
  }
  if (tag==1) (void)mulib_xsimd_scale(simd,pcm.samples,pcm.samples,frames*channels,(float)ldexp(1.0,1-(int)bits));
  *out=pcm; return MULIB_AUDIO_OK;
}
mulib_audio_status_t mulib_miniwav_encode (const mulib_audio_pcm_t *pcm, unsigned bits,
    int floating, const mulib_audio_options_t *opt, mulib_audio_bytes_t *out)
{
  unsigned char h[44]={0}, block[4096]; size_t bytes, i, n, j, width; mulib_audio_bytes_t b={0};
  mulib_audio_status_t st;
  if (!out || out->data || out->size || out->capacity) return MULIB_AUDIO_INVALID;
  st=mulib_audio_validate(pcm,opt); if (st) return st;
  if ((floating && bits!=32) || (!floating && bits!=8 && bits!=16 && bits!=24 && bits!=32)
      || sizeof(float)!=4 || FLT_RADIX!=2) return MULIB_AUDIO_UNSUPPORTED;
  width=bits/8;
  if (pcm->frames>(UINT32_MAX-37)/(pcm->channels*width)) return MULIB_AUDIO_LIMIT;
  bytes=pcm->frames*pcm->channels*width;
  memcpy(h,"RIFF",4); mulib_miniwav_put_(h+4,(uint32_t)(36+bytes+(bytes&1)),4); memcpy(h+8,"WAVEfmt ",8);
  mulib_miniwav_put_(h+16,16,4); mulib_miniwav_put_(h+20,floating ? 3 : 1,2);
  mulib_miniwav_put_(h+22,pcm->channels,2); mulib_miniwav_put_(h+24,pcm->rate,4);
  mulib_miniwav_put_(h+28,(uint32_t)(pcm->rate*pcm->channels*width),4);
  mulib_miniwav_put_(h+32,(uint32_t)(pcm->channels*width),2); mulib_miniwav_put_(h+34,bits,2);
  memcpy(h+36,"data",4); mulib_miniwav_put_(h+40,(uint32_t)bytes,4);
  st=mulib_audio_append(&b,h,44);
  for (i=0; !st && i<pcm->frames*pcm->channels; i+=n) {
    float scaled[1024];
    n=pcm->frames*pcm->channels-i; if (n>1024) n=1024;
    (void)mulib_xsimd_scale(opt ? opt->simd : MULIB_XSIMD_AUTO,scaled,pcm->samples+i,n,
                          floating ? 1.0f : (float)ldexp(1.0,(int)bits-1));
    for(j=0;j<n;++j) {
      uint32_t u;
      if(floating) memcpy(&u,scaled+j,4);
      else {
        double v=scaled[j], high=ldexp(1.0,(int)bits-1)-1, low=-ldexp(1.0,(int)bits-1);
        int64_t q; if(v>high) v=high; if(v<low) v=low;
        q=(int64_t)round(v); if(bits==8) q+=128; u=(uint32_t)q;
      }
      mulib_miniwav_put_(block+j*width,u,(unsigned)width);
    }
    st=mulib_audio_append(&b,block,n*width);
  }
  if(!st && (bytes&1)) { unsigned char zero=0; st=mulib_audio_append(&b,&zero,1); }
  if(st) mulib_audio_bytes_free(&b); else *out=b;
  return st;
}
#endif
