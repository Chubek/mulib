/** @file mulib-miniflac.h
 * @brief Native FLAC lossless decoder and verbatim-subframe encoder.
 */
#ifndef MULIB_MINIFLAC_H
#define MULIB_MINIFLAC_H
#include "mulib-audio.h"
#ifdef __cplusplus
extern "C" {
#endif
/** Decode native FLAC, 4..24-bit, up to eight channels, constant/verbatim/fixed/
 * LPC subframes, Rice/Rice2 and stereo decorrelation. Checks frame CRCs.
 * STREAMINFO MD5 is not verified. Ogg FLAC is unsupported. Output must be empty. */
mulib_audio_status_t mulib_miniflac_decode (const void *, size_t,
    const mulib_audio_options_t *, mulib_audio_pcm_t *);
/** Encode native FLAC with independent verbatim subframes and 4096-frame blocks.
 * bits must be 16 or 24. MD5 is marked unavailable (all zero), as permitted by
 * FLAC. Lossless with respect to rounded/clipped integer PCM. Empty PCM allowed.
 * SIMD is used for PCM scaling. Output must be empty; failures preserve it. */
mulib_audio_status_t mulib_miniflac_encode (const mulib_audio_pcm_t *, unsigned bits,
    const mulib_audio_options_t *, mulib_audio_bytes_t *);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_MINIFLAC) && !defined(MULIB_MINIFLAC_IMPLEMENTED)
#define MULIB_MINIFLAC_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
typedef struct mulib_miniflac_bits_ { const unsigned char *p; size_t n, at; int bad; } mulib_miniflac_bits_;
static uint64_t mulib_miniflac_get_ (mulib_miniflac_bits_ *b, unsigned n)
{
  uint64_t v=0; unsigned i;
  if(n>64 || b->at>b->n || n>b->n-b->at) { b->bad=1; return 0; }
  for(i=0;i<n;++i,++b->at) v=(v<<1)|((b->p[b->at/8]>>(7-b->at%8))&1);
  return v;
}
static int64_t mulib_miniflac_signed_ (mulib_miniflac_bits_ *b, unsigned n)
{
  uint64_t v=mulib_miniflac_get_(b,n);
  return n && (v & (UINT64_C(1)<<(n-1))) ? (int64_t)(v-(UINT64_C(1)<<(n-1)))-(INT64_C(1)<<(n-1)) : (int64_t)v;
}
static unsigned mulib_miniflac_crc_ (const unsigned char *p, size_t n, unsigned width, unsigned poly)
{
  unsigned crc=0, j, top=1u<<(width-1), mask=(1u<<width)-1; size_t i;
  for(i=0;i<n;++i) { crc^=(unsigned)p[i]<<(width-8); for(j=0;j<8;++j) crc=((crc<<1)^((crc&top)?poly:0))&mask; }
  return crc;
}
static void mulib_miniflac_be_ (unsigned char *p, uint64_t v, unsigned n)
{ while(n) { p[--n]=(unsigned char)v; v>>=8; } }
static mulib_audio_status_t mulib_miniflac_residual_ (mulib_miniflac_bits_ *b, int64_t *x, size_t n, unsigned order)
{
  unsigned method=(unsigned)mulib_miniflac_get_(b,2), po=(unsigned)mulib_miniflac_get_(b,4), part;
  size_t at=order, count;
  if(method>1) return MULIB_AUDIO_UNSUPPORTED;
  if((n&(((size_t)1<<po)-1)) || (n>>po)<order) return MULIB_AUDIO_FORMAT;
  count=n>>po;
  for(part=0;part<(1u<<po);++part) {
    unsigned bits=method ? 5 : 4, k=(unsigned)mulib_miniflac_get_(b,bits), raw=0;
    size_t j, amount=count-(part==0 ? order : 0);
    if(k==((1u<<bits)-1)) { raw=1; k=(unsigned)mulib_miniflac_get_(b,5); }
    for(j=0;j<amount;++j) {
      if(raw) x[at++]=mulib_miniflac_signed_(b,k);
      else {
        uint64_t q=0, u;
        while(!b->bad && !mulib_miniflac_get_(b,1)) { if(++q>(UINT64_C(1)<<32)) return MULIB_AUDIO_LIMIT; }
        if(b->bad) return MULIB_AUDIO_FORMAT;
        u=(q<<k)|mulib_miniflac_get_(b,k);
        if(u>UINT32_MAX) return MULIB_AUDIO_FORMAT;
        x[at++]=(u&1) ? -(int64_t)(u/2)-1 : (int64_t)(u/2);
      }
      if(b->bad) return MULIB_AUDIO_FORMAT;
    }
  }
  return MULIB_AUDIO_OK;
}
static mulib_audio_status_t mulib_miniflac_sub_ (mulib_miniflac_bits_ *b, int64_t *x, size_t n, unsigned bits)
{
  unsigned type, wasted=0, order=0, i, precision=0; int shift=0; int64_t coeff[32]; size_t j;
  mulib_audio_status_t st;
  if(mulib_miniflac_get_(b,1)) return MULIB_AUDIO_FORMAT;
  type=(unsigned)mulib_miniflac_get_(b,6);
  if(mulib_miniflac_get_(b,1)) {
    wasted=1;
    while(!b->bad && !mulib_miniflac_get_(b,1)) { if(++wasted>=bits) return MULIB_AUDIO_FORMAT; }
  }
  if(b->bad || wasted>=bits) return MULIB_AUDIO_FORMAT;
  bits-=wasted;
  if(type==0) { int64_t v=mulib_miniflac_signed_(b,bits); for(j=0;j<n;++j) x[j]=v; }
  else if(type==1) { for(j=0;j<n;++j) x[j]=mulib_miniflac_signed_(b,bits); }
  else {
    if(type>=8 && type<=12) order=type-8;
    else if(type>=32) order=type-31;
    else return MULIB_AUDIO_FORMAT;
    if(order>n) return MULIB_AUDIO_FORMAT;
    for(i=0;i<order;++i) x[i]=mulib_miniflac_signed_(b,bits);
    if(type>=32) {
      precision=(unsigned)mulib_miniflac_get_(b,4)+1;
      if(precision==16) return MULIB_AUDIO_FORMAT;
      shift=(int)mulib_miniflac_signed_(b,5);
      for(i=0;i<order;++i) coeff[i]=mulib_miniflac_signed_(b,precision);
    } else {
      static const int c[5][4]={{0,0,0,0},{1,0,0,0},{2,-1,0,0},{3,-3,1,0},{4,-6,4,-1}};
      for(i=0;i<order;++i) coeff[i]=c[order][i];
    }
    if(b->bad) return MULIB_AUDIO_FORMAT;
    st=mulib_miniflac_residual_(b,x,n,order); if(st) return st;
    for(j=order;j<n;++j) {
      int64_t sum=0, v;
      for(i=0;i<order;++i) sum+=coeff[i]*x[j-i-1];
      if(shift>=0) { int64_t d=INT64_C(1)<<shift; sum=sum>=0 ? sum/d : -((-sum+d-1)/d); }
      else sum*=INT64_C(1)<<-shift;
      v=x[j]+sum;
      if(v<-(INT64_C(1)<<(bits-1)) || v>=(INT64_C(1)<<(bits-1))) return MULIB_AUDIO_FORMAT;
      x[j]=v;
    }
  }
  if(b->bad) return MULIB_AUDIO_FORMAT;
  for(j=0;j<n;++j) x[j]*=INT64_C(1)<<wasted;
  return MULIB_AUDIO_OK;
}
mulib_audio_status_t mulib_miniflac_decode (const void *data, size_t size,
    const mulib_audio_options_t *opt, mulib_audio_pcm_t *out)
{
  const unsigned char *p=(const unsigned char *)data; size_t at=4, frames=0, max=opt&&opt->max_frames?opt->max_frames:16777216;
  unsigned rate=0, channels=0, bits=0, last=0, blockmax=0; uint64_t total=0, frame_no=0;
  mulib_audio_bytes_t pcm={0}; mulib_audio_status_t st=MULIB_AUDIO_FORMAT;
  mulib_xsimd_backend_t simd=opt?opt->simd:MULIB_XSIMD_AUTO;
  if(!out || out->samples || out->frames || (!data && size)) return MULIB_AUDIO_INVALID;
  if(!mulib_xsimd_available(simd)) return MULIB_AUDIO_UNSUPPORTED;
  if(size<4 || memcmp(p,"fLaC",4)) return MULIB_AUDIO_FORMAT;
  while(!last) {
    unsigned type; size_t n;
    if(size-at<4) goto fail;
    last=p[at]>>7; type=p[at]&127; n=((size_t)p[at+1]<<16)|((size_t)p[at+2]<<8)|p[at+3]; at+=4;
    if(n>size-at || type==127 || (!rate && type!=0)) goto fail;
    if(type==0) {
      mulib_miniflac_bits_ b;
      if(rate || n!=34) goto fail;
      b.p=p+at; b.n=n*8; b.at=0; b.bad=0;
      if(mulib_miniflac_get_(&b,16)<16) goto fail;
      blockmax=(unsigned)mulib_miniflac_get_(&b,16); if(blockmax<16) goto fail;
      (void)mulib_miniflac_get_(&b,48); rate=(unsigned)mulib_miniflac_get_(&b,20);
      channels=(unsigned)mulib_miniflac_get_(&b,3)+1; bits=(unsigned)mulib_miniflac_get_(&b,5)+1; total=mulib_miniflac_get_(&b,36);
      if(!rate || rate>768000 || bits<4 || bits>24) { st=MULIB_AUDIO_UNSUPPORTED; goto fail; }
      if(total>max) { st=MULIB_AUDIO_LIMIT; goto fail; }
    }
    at+=n;
  }
  while(at<size) {
    mulib_miniflac_bits_ b; unsigned variable, bc, rc, ca, sc, fb, ch, sample_rate;
    uint64_t number; size_t n, start=at, i; int64_t *x=NULL; float *f=NULL;
    if(size-at>SIZE_MAX/8) { st=MULIB_AUDIO_LIMIT; goto fail; }
    b.p=p+at; b.n=(size-at)*8; b.at=0; b.bad=0;
    if(mulib_miniflac_get_(&b,14)!=0x3ffe || mulib_miniflac_get_(&b,1)) goto fail;
    variable=(unsigned)mulib_miniflac_get_(&b,1); bc=(unsigned)mulib_miniflac_get_(&b,4); rc=(unsigned)mulib_miniflac_get_(&b,4);
    ca=(unsigned)mulib_miniflac_get_(&b,4); sc=(unsigned)mulib_miniflac_get_(&b,3);
    if(mulib_miniflac_get_(&b,1) || !bc || rc==15 || ca>10 || sc==3 || sc==7) goto fail;
    ch=ca<8 ? ca+1 : 2; if(ch!=channels) goto fail;
    { static const unsigned depths[8]={0,8,12,0,16,20,24,0}; fb=sc?depths[sc]:bits; }
    if(fb!=bits) goto fail;
    {
      unsigned lead=(unsigned)mulib_miniflac_get_(&b,8), count=0, j;
      if(lead<128) number=lead;
      else {
        unsigned mask=128;
        while(lead&mask) { ++count; mask>>=1; if(!mask) break; }
        if(count<2 || count>7) goto fail;
        number=lead&(mask-1);
        for(j=1;j<count;++j) { unsigned c=(unsigned)mulib_miniflac_get_(&b,8); if((c&192)!=128) goto fail; number=(number<<6)|(c&63); }
        if(number<(count==2 ? 128 : (UINT64_C(1)<<(5*count-4)))) goto fail;
      }
    }
    if(number!=(variable ? frames : frame_no)) goto fail;
    if(bc==1) n=192;
    else if(bc<=5) n=(size_t)576<<(bc-2);
    else if(bc==6 || bc==7) n=(size_t)mulib_miniflac_get_(&b,bc==6?8:16)+1;
    else n=(size_t)256<<(bc-8);
    if(n>blockmax) goto fail;
    { static const unsigned rates[12]={0,88200,176400,192000,8000,16000,22050,24000,32000,44100,48000,96000};
      sample_rate=rc<12 ? (rc?rates[rc]:rate) : (unsigned)mulib_miniflac_get_(&b,rc==12?8:16)*(rc==12?1000:rc==14?10:1); }
    if(sample_rate!=rate || b.bad || b.at/8>=size-at) goto fail;
    { unsigned crc=mulib_miniflac_crc_(p+at,b.at/8,8,7);
      if(crc!=mulib_miniflac_get_(&b,8)) goto fail; }
    if(n>max-frames || n>SIZE_MAX/ch/sizeof(int64_t)) { st=MULIB_AUDIO_LIMIT; goto fail; }
    x=(int64_t *)malloc(n*ch*sizeof(*x)); f=(float *)malloc(n*ch*sizeof(*f));
    if(!x || !f) { free(x); free(f); st=MULIB_AUDIO_MEMORY; goto fail; }
    for(i=0;i<ch;++i) {
      unsigned depth=bits+((ca==8 && i==1) || (ca==9 && i==0) || (ca==10 && i==1));
      st=mulib_miniflac_sub_(&b,x+i*n,n,depth); if(st) break;
    }
    if(!st) for(i=0;i<n;++i) {
      unsigned c;
      if(ca==8) x[n+i]=x[i]-x[n+i];
      else if(ca==9) x[i]+=x[n+i];
      else if(ca==10) { int64_t mid=x[i]*2+(x[n+i]&1), side=x[n+i]; x[i]=(mid+side)/2; x[n+i]=(mid-side)/2; }
      for(c=0;c<ch;++c) {
        if(x[c*n+i]<-(INT64_C(1)<<(bits-1)) || x[c*n+i]>=(INT64_C(1)<<(bits-1))) { st=MULIB_AUDIO_FORMAT; break; }
        f[i*ch+c]=(float)x[c*n+i];
      }
      if(st) break;
    }
    free(x);
    if(!st) {
      unsigned pad=(unsigned)((8-b.at%8)%8);
      if(mulib_miniflac_get_(&b,pad)) st=MULIB_AUDIO_FORMAT;
      at=start+b.at/8;
      if(b.bad || size-at<2 || mulib_miniflac_crc_(p+start,at-start,16,0x8005)!=mulib_miniflac_get_(&b,16)) st=MULIB_AUDIO_FORMAT;
      at+=2;
    }
    if(!st) { (void)mulib_xsimd_scale(simd,f,f,n*ch,(float)ldexp(1.0,1-(int)bits)); st=mulib_audio_append(&pcm,f,n*ch*sizeof(float)); }
    free(f); if(st) goto fail;
    frames+=n; ++frame_no; st=MULIB_AUDIO_FORMAT;
  }
  if(total && frames!=total) goto fail;
  out->samples=(float *)pcm.data; out->frames=frames; out->rate=rate; out->channels=channels;
  return MULIB_AUDIO_OK;
fail:
  mulib_audio_bytes_free(&pcm); return st;
}
mulib_audio_status_t mulib_miniflac_encode (const mulib_audio_pcm_t *pcm, unsigned bits,
    const mulib_audio_options_t *opt, mulib_audio_bytes_t *out)
{
  unsigned char h[42]={0}; mulib_audio_bytes_t b={0}; mulib_audio_status_t st;
  size_t at, frame=0; unsigned width=bits/8;
  if(!out || out->data || out->size || out->capacity) return MULIB_AUDIO_INVALID;
  st=mulib_audio_validate(pcm,opt); if(st) return st;
  if(bits!=16 && bits!=24) return MULIB_AUDIO_UNSUPPORTED;
  if(pcm->frames>UINT64_C(0xfffffffff)) return MULIB_AUDIO_LIMIT;
  memcpy(h,"fLaC",4); h[4]=128; h[7]=34; h[8]=16; h[10]=16;
  mulib_miniflac_be_(h+18,((uint64_t)pcm->rate<<44)|((uint64_t)(pcm->channels-1)<<41)|((uint64_t)(bits-1)<<36)|pcm->frames,8);
  st=mulib_audio_append(&b,h,sizeof(h));
  for(at=0;!st && at<pcm->frames;at+=4096,++frame) {
    unsigned char header[16], *body; size_t n=pcm->frames-at, hs=4, start=b.size, i; unsigned c;
    if(n>4096) n=4096;
    header[0]=255; header[1]=248; header[2]=0x70; header[3]=(unsigned char)(((pcm->channels-1)<<4)|(bits==16?8:12));
    if(frame<128) header[hs++]=(unsigned char)frame;
    else {
      unsigned count=2, j; uint64_t v=frame;
      while(count<7 && v>=(UINT64_C(1)<<(5*count+1))) ++count;
      header[hs++]=(unsigned char)((256-(1u<<(8-count)))|(v>>(6*(count-1))));
      for(j=count-1;j;--j) header[hs++]=(unsigned char)(128|((v>>(6*(j-1)))&63));
    }
    mulib_miniflac_be_(header+hs,n-1,2); hs+=2;
    header[hs]=(unsigned char)mulib_miniflac_crc_(header,hs,8,7); ++hs;
    st=mulib_audio_append(&b,header,hs); if(st) break;
    body=(unsigned char *)malloc(1+n*width); if(!body) { st=MULIB_AUDIO_MEMORY; break; }
    body[0]=2;
    for(c=0;!st && c<pcm->channels;++c) {
      float scaled[4096];
      for(i=0;i<n;++i) scaled[i]=pcm->samples[(at+i)*pcm->channels+c];
      (void)mulib_xsimd_scale(opt?opt->simd:MULIB_XSIMD_AUTO,scaled,scaled,n,(float)ldexp(1.0,(int)bits-1));
      for(i=0;i<n;++i) {
        double v=scaled[i], hi=ldexp(1.0,(int)bits-1)-1, lo=-ldexp(1.0,(int)bits-1);
        if(v>hi) v=hi;
        if(v<lo) v=lo;
        mulib_miniflac_be_(body+1+i*width,(uint64_t)(int64_t)round(v),width);
      }
      st=mulib_audio_append(&b,body,1+n*width);
    }
    free(body);
    if(!st) { unsigned char crc[2]; mulib_miniflac_be_(crc,mulib_miniflac_crc_(b.data+start,b.size-start,16,0x8005),2); st=mulib_audio_append(&b,crc,2); }
  }
  if(st) mulib_audio_bytes_free(&b); else *out=b;
  return st;
}
#endif
