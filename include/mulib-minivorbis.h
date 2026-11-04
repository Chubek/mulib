/** @file mulib-minivorbis.h
 * @brief Original fixed-block Ogg/Vorbis encoder and matching profile decoder.
 */
#ifndef MULIB_MINIVORBIS_H
#define MULIB_MINIVORBIS_H
#include "mulib-audio.h"
#ifdef __cplusplus
extern "C" {
#endif
/** Encode mono/stereo Vorbis I in Ogg: 256-sample windows, floor1, residue1,
 * uncoupled channels, scalar 8-bit spectral VQ. This simple lossy encoder has
 * no psychoacoustic bitrate control. Rate 8000..192000. Output must be empty.
 * SIMD accelerates MDCT dot products; no external codec code is used. */
mulib_audio_status_t mulib_minivorbis_encode (const mulib_audio_pcm_t *,
    const mulib_audio_options_t *, mulib_audio_bytes_t *);
/** Decode the setup profile emitted above, including floor endpoint variation.
 * Other Vorbis setup configurations return UNSUPPORTED, not guessed audio.
 * Supports Ogg packet lacing/continuation, CRC and final granule trimming;
 * chained/multiplexed streams are unsupported. Output must be empty. */
mulib_audio_status_t mulib_minivorbis_decode (const void *, size_t,
    const mulib_audio_options_t *, mulib_audio_pcm_t *);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_MINIVORBIS) && !defined(MULIB_MINIVORBIS_IMPLEMENTED)
#define MULIB_MINIVORBIS_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
typedef struct mulib_minivorbis_bits_ { unsigned char *p; size_t n, at; int bad; } mulib_minivorbis_bits_;
static void mulib_minivorbis_put_ (mulib_minivorbis_bits_ *b, uint32_t v, unsigned n)
{
  unsigned i; if(b->at>b->n*8 || n>b->n*8-b->at) { b->bad=1; return; }
  for(i=0;i<n;++i,++b->at) if((v>>i)&1) b->p[b->at/8]|=(unsigned char)(1u<<(b->at%8));
}
static uint32_t mulib_minivorbis_get_ (mulib_minivorbis_bits_ *b, unsigned n)
{
  uint32_t v=0; unsigned i; if(b->at>b->n*8 || n>b->n*8-b->at) { b->bad=1; return 0; }
  for(i=0;i<n;++i,++b->at) v|=(uint32_t)((b->p[b->at/8]>>(b->at%8))&1)<<i;
  return v;
}
static uint64_t mulib_minivorbis_le_ (const unsigned char *p, unsigned n)
{ uint64_t v=0; unsigned i; for(i=0;i<n;++i) v|=(uint64_t)p[i]<<(8*i); return v; }
static void mulib_minivorbis_store_ (unsigned char *p, uint64_t v, unsigned n)
{ unsigned i; for(i=0;i<n;++i) p[i]=(unsigned char)(v>>(i*8)); }
static uint32_t mulib_minivorbis_crc_ (const unsigned char *p, size_t n)
{
  uint32_t crc=0; size_t i; unsigned j;
  for(i=0;i<n;++i) { crc^=(uint32_t)(i>=22&&i<26?0:p[i])<<24; for(j=0;j<8;++j) crc=(crc<<1)^((crc>>31)?UINT32_C(0x04c11db7):0); }
  return crc;
}
static unsigned mulib_minivorbis_reverse_ (unsigned v)
{ unsigned r=0, i; for(i=0;i<8;++i) { r=(r<<1)|(v&1); v>>=1; } return r; }
/* Codebooks and setup are authored directly from the Vorbis I bitstream spec.
 * These are format data, not imported implementation sources. */
static size_t mulib_minivorbis_setup_ (unsigned char *p)
{
  mulib_minivorbis_bits_ b; unsigned i;
  memset(p,0,1024); p[0]=5; memcpy(p+1,"vorbis",6); b.p=p; b.n=1024; b.at=56; b.bad=0;
  mulib_minivorbis_put_(&b,1,8); /* two books */
  mulib_minivorbis_put_(&b,0x564342,24); mulib_minivorbis_put_(&b,1,16); mulib_minivorbis_put_(&b,1,24);
  mulib_minivorbis_put_(&b,0,1); mulib_minivorbis_put_(&b,0,1); mulib_minivorbis_put_(&b,0,5); mulib_minivorbis_put_(&b,0,4);
  mulib_minivorbis_put_(&b,0x564342,24); mulib_minivorbis_put_(&b,1,16); mulib_minivorbis_put_(&b,256,24);
  mulib_minivorbis_put_(&b,1,1); mulib_minivorbis_put_(&b,7,5); mulib_minivorbis_put_(&b,256,9);
  mulib_minivorbis_put_(&b,1,4);
  /* Vorbis floats: signed 21-bit mantissa times 2^(exponent-788). */
  mulib_minivorbis_put_(&b,UINT32_C(0x80000000)|(788u<<21)|1,32); /* -1 */
  mulib_minivorbis_put_(&b,(781u<<21)|1,32); /* 1/128 */
  mulib_minivorbis_put_(&b,7,4); mulib_minivorbis_put_(&b,0,1);
  for(i=0;i<256;++i) mulib_minivorbis_put_(&b,i,8);
  mulib_minivorbis_put_(&b,0,6); mulib_minivorbis_put_(&b,0,16); /* time */
  mulib_minivorbis_put_(&b,0,6); mulib_minivorbis_put_(&b,1,16); /* floor1 */
  mulib_minivorbis_put_(&b,0,5); mulib_minivorbis_put_(&b,0,2); mulib_minivorbis_put_(&b,7,4);
  mulib_minivorbis_put_(&b,0,6); mulib_minivorbis_put_(&b,1,16); /* residue1 */
  mulib_minivorbis_put_(&b,0,24); mulib_minivorbis_put_(&b,128,24); mulib_minivorbis_put_(&b,15,24);
  mulib_minivorbis_put_(&b,0,6); mulib_minivorbis_put_(&b,0,8); /* one class, book0 */
  mulib_minivorbis_put_(&b,1,3); mulib_minivorbis_put_(&b,0,1); mulib_minivorbis_put_(&b,1,8);
  mulib_minivorbis_put_(&b,0,6); mulib_minivorbis_put_(&b,0,16); /* mapping0 */
  mulib_minivorbis_put_(&b,0,1); mulib_minivorbis_put_(&b,0,1); mulib_minivorbis_put_(&b,0,2);
  mulib_minivorbis_put_(&b,0,8); mulib_minivorbis_put_(&b,0,8); mulib_minivorbis_put_(&b,0,8);
  mulib_minivorbis_put_(&b,0,6); mulib_minivorbis_put_(&b,0,1); /* one short mode */
  mulib_minivorbis_put_(&b,0,16); mulib_minivorbis_put_(&b,0,16); mulib_minivorbis_put_(&b,0,8);
  mulib_minivorbis_put_(&b,1,1); return (b.at+7)/8;
}
static mulib_audio_status_t mulib_minivorbis_page_ (mulib_audio_bytes_t *out, const unsigned char *packet,
    size_t n, uint32_t sequence, uint64_t granule, unsigned flags)
{
  unsigned char h[282]={0}; size_t seg=n/255+1, i, start=out->size; mulib_audio_status_t st;
  if(seg>255) return MULIB_AUDIO_LIMIT;
  memcpy(h,"OggS",4); h[5]=(unsigned char)flags; mulib_minivorbis_store_(h+6,granule,8);
  mulib_minivorbis_store_(h+14,0x4d754c42,4); mulib_minivorbis_store_(h+18,sequence,4); h[26]=(unsigned char)seg;
  for(i=0;i<seg;++i) h[27+i]=(unsigned char)(i+1<seg?255:n%255);
  st=mulib_audio_append(out,h,27+seg); if(!st) st=mulib_audio_append(out,packet,n);
  if(!st) mulib_minivorbis_store_(out->data+start+22,mulib_minivorbis_crc_(out->data+start,out->size-start),4);
  return st;
}
static float *mulib_minivorbis_matrix_ (int inverse)
{
  float *m=(float *)malloc(256*128*sizeof(float)); unsigned i,k;
  if(!m) return NULL;
  for(i=0;i<256;++i) {
    double w=sin(3.14159265358979323846*(i+0.5)/256); w=sin(1.57079632679489661923*w*w);
    for(k=0;k<128;++k) {
      float v=(float)(w*cos(3.14159265358979323846/128*(i+0.5+64)*(k+0.5)));
      m[inverse?i*128+k:k*256+i]=v;
    }
  }
  return m;
}
static double mulib_minivorbis_floor_ (unsigned y)
{ return exp(((int)y-255)*0.06296130868913505); }
mulib_audio_status_t mulib_minivorbis_encode (const mulib_audio_pcm_t *p, const mulib_audio_options_t *opt, mulib_audio_bytes_t *out)
{
  mulib_audio_bytes_t b={0}; unsigned char packet[1024]={0}; size_t block, blocks, size; float *matrix;
  mulib_audio_status_t st; mulib_xsimd_backend_t simd=opt?opt->simd:MULIB_XSIMD_AUTO;
  if(!out || out->data || out->size || out->capacity) return MULIB_AUDIO_INVALID;
  st=mulib_audio_validate(p,opt); if(st) return st;
  if(p->channels>2 || p->rate<8000 || p->rate>192000) return MULIB_AUDIO_UNSUPPORTED;
  if(p->frames>SIZE_MAX-127 || (p->frames+127)/128>UINT32_MAX-4) return MULIB_AUDIO_LIMIT;
  packet[0]=1; memcpy(packet+1,"vorbis",6); packet[11]=(unsigned char)p->channels;
  mulib_minivorbis_store_(packet+12,p->rate,4); packet[28]=0x88; packet[29]=1;
  st=mulib_minivorbis_page_(&b,packet,30,0,0,2);
  memset(packet,0,sizeof(packet)); packet[0]=3; memcpy(packet+1,"vorbis",6); packet[7]=5; memcpy(packet+11,"MuLIB",5); packet[20]=1;
  if(!st) st=mulib_minivorbis_page_(&b,packet,21,1,0,0);
  size=mulib_minivorbis_setup_(packet); if(!st) st=mulib_minivorbis_page_(&b,packet,size,2,0,0);
  matrix=mulib_minivorbis_matrix_(0); if(!matrix) st=MULIB_AUDIO_MEMORY;
  blocks=(p->frames+127)/128+1;
  for(block=0;!st && block<blocks;++block) {
    float spectrum[2][128]; unsigned y[2], c,k,i,part; mulib_minivorbis_bits_ bits;
    memset(packet,0,sizeof(packet)); bits.p=packet; bits.n=sizeof(packet); bits.at=0; bits.bad=0;
    mulib_minivorbis_put_(&bits,0,1);
    for(c=0;c<p->channels;++c) {
      float input[256]; double peak=0;
      for(i=0;i<256;++i) {
        size_t pos=block*128+i; input[i]=0;
        if(pos>=128 && pos-128<p->frames) { float v=p->samples[(pos-128)*p->channels+c]; input[i]=v>1?1:v<-1?-1:v; }
      }
      for(k=0;k<128;++k) { (void)mulib_xsimd_dot(simd,input,matrix+k*256,256,&spectrum[c][k]); spectrum[c][k]*=1.0f/64;
        if(fabs(spectrum[c][k])>peak) peak=fabs(spectrum[c][k]); }
      if(peak<1e-8) y[c]=0;
      else { double v=ceil(255+log(peak*1.01)/0.06296130868913505); y[c]=(unsigned)(v<0?0:v>255?255:v); }
      mulib_minivorbis_put_(&bits,1,1); mulib_minivorbis_put_(&bits,y[c],8); mulib_minivorbis_put_(&bits,y[c],8);
    }
    for(part=0;part<8;++part) {
      for(c=0;c<p->channels;++c) mulib_minivorbis_put_(&bits,0,1); /* classifications */
      for(c=0;c<p->channels;++c) for(k=part*16;k<(part+1)*16;++k) {
        double v=round(spectrum[c][k]/mulib_minivorbis_floor_(y[c])*128)+128;
        unsigned q=(unsigned)(v<0?0:v>255?255:v); mulib_minivorbis_put_(&bits,mulib_minivorbis_reverse_(q),8);
      }
    }
    st=mulib_minivorbis_page_(&b,packet,(bits.at+7)/8,(uint32_t)block+3,
          block+1==blocks?p->frames:block*128,block+1==blocks?4:0);
  }
  free(matrix); if(st) mulib_audio_bytes_free(&b); else *out=b; return st;
}
typedef struct mulib_minivorbis_decode_ {
  unsigned headers, channels, rate; size_t blocks, frames, max;
  float previous[2][128], *matrix;
  mulib_xsimd_backend_t simd; mulib_audio_bytes_t pcm;
} mulib_minivorbis_decode_;
static mulib_audio_status_t mulib_minivorbis_packet_ (mulib_minivorbis_decode_ *d, unsigned char *p, size_t n)
{
  unsigned char setup[1024]; size_t expected; unsigned c,k,i,part; float spectrum[2][128]={{0}}, block[256];
  unsigned present[2]={0}, y0[2]={0}, y1[2]={0}; mulib_minivorbis_bits_ bits;
  if(d->headers<3) {
    if(n<7 || memcmp(p+1,"vorbis",6) || p[0]!=(d->headers*2+1)) return MULIB_AUDIO_FORMAT;
    if(!d->headers) {
      if(n!=30 || mulib_minivorbis_le_(p+7,4) || !p[11] || !mulib_minivorbis_le_(p+12,4) || !(p[29]&1)) return MULIB_AUDIO_FORMAT;
      d->channels=p[11]; d->rate=(unsigned)mulib_minivorbis_le_(p+12,4);
      if(p[28]!=0x88 || d->channels>2 || d->rate>192000 || d->rate<8000) return MULIB_AUDIO_UNSUPPORTED;
    } else if(d->headers==1) {
      size_t at=7, length; uint32_t count,j;
      if(n-at<4) return MULIB_AUDIO_FORMAT;
      length=(size_t)mulib_minivorbis_le_(p+at,4); at+=4; if(length>n-at) return MULIB_AUDIO_FORMAT; at+=length;
      if(n-at<4) return MULIB_AUDIO_FORMAT;
      count=(uint32_t)mulib_minivorbis_le_(p+at,4); at+=4;
      for(j=0;j<count;++j) { if(n-at<4) return MULIB_AUDIO_FORMAT; length=(size_t)mulib_minivorbis_le_(p+at,4); at+=4;
        if(length>n-at) return MULIB_AUDIO_FORMAT;
        at+=length; }
      if(at>=n || !(p[at]&1)) return MULIB_AUDIO_FORMAT;
    } else {
      expected=mulib_minivorbis_setup_(setup);
      if(n!=expected || memcmp(p,setup,n)) return MULIB_AUDIO_UNSUPPORTED;
    }
    ++d->headers; return MULIB_AUDIO_OK;
  }
  if(!n || n>SIZE_MAX/8) return MULIB_AUDIO_FORMAT;
  bits.p=p; bits.n=n; bits.at=0; bits.bad=0;
  if(mulib_minivorbis_get_(&bits,1)) return MULIB_AUDIO_UNSUPPORTED;
  for(c=0;c<d->channels;++c) {
    present[c]=mulib_minivorbis_get_(&bits,1);
    if(present[c]) { y0[c]=mulib_minivorbis_get_(&bits,8); y1[c]=mulib_minivorbis_get_(&bits,8); }
  }
  for(part=0;part<8;++part) {
    for(c=0;c<d->channels;++c) if(present[c] && mulib_minivorbis_get_(&bits,1)) return MULIB_AUDIO_FORMAT;
    for(c=0;c<d->channels;++c) if(present[c]) for(k=part*16;k<(part+1)*16;++k) {
      unsigned q=mulib_minivorbis_reverse_(mulib_minivorbis_get_(&bits,8));
      spectrum[c][k]=((int)q-128)/128.0f;
    }
  }
  if(bits.bad) return MULIB_AUDIO_FORMAT;
  if(d->blocks && (d->frames>d->max || 128>d->max-d->frames)) return MULIB_AUDIO_LIMIT;
  for(c=0;c<d->channels;++c) {
    int dy=(int)y1[c]-(int)y0[c], base=dy/128, sy=dy<0?base-1:base+1, ady=abs(dy)-abs(base)*128, err=0, y=(int)y0[c];
    for(k=0;k<128;++k) {
      spectrum[c][k]*=(float)mulib_minivorbis_floor_((unsigned)y);
      err+=ady; if(err>=128) { err-=128; y+=sy; } else y+=base;
    }
    for(i=0;i<256;++i) {
      float v; (void)mulib_xsimd_dot(d->simd,spectrum[c],d->matrix+i*128,128,&v);
      if(i<128) block[i*d->channels+c]=v+d->previous[c][i]; else d->previous[c][i-128]=v;
    }
  }
  if(d->blocks++) { mulib_audio_status_t st=mulib_audio_append(&d->pcm,block,128*d->channels*sizeof(float)); if(st) return st; d->frames+=128; }
  return MULIB_AUDIO_OK;
}
mulib_audio_status_t mulib_minivorbis_decode (const void *data, size_t size, const mulib_audio_options_t *opt, mulib_audio_pcm_t *out)
{
  const unsigned char *p=(const unsigned char *)data; mulib_minivorbis_decode_ d; mulib_audio_bytes_t packet={0};
  size_t at=0; uint32_t serial=0, seq=0; uint64_t last_granule=0; int eos=0; mulib_audio_status_t st=MULIB_AUDIO_FORMAT;
  if(!out || out->samples || out->frames || (!data && size)) return MULIB_AUDIO_INVALID;
  memset(&d,0,sizeof(d)); d.simd=opt?opt->simd:MULIB_XSIMD_AUTO; d.max=opt&&opt->max_frames?opt->max_frames:16777216;
  if(!mulib_xsimd_available(d.simd)) return MULIB_AUDIO_UNSUPPORTED;
  /* One block of final padding may exceed the requested final-frame limit. */
  if(d.max<=SIZE_MAX-127) d.max+=127;
  d.matrix=mulib_minivorbis_matrix_(1); if(!d.matrix) return MULIB_AUDIO_MEMORY;
  while(at<size) {
    size_t nseg, length, i, payload; unsigned flags; uint64_t granule; int completed=0;
    if(eos || size-at<27 || memcmp(p+at,"OggS",4) || p[at+4]) goto done;
    flags=p[at+5]; if(flags&~7u) goto done;
    if(!at) { if(flags!=2) goto done; serial=(uint32_t)mulib_minivorbis_le_(p+at+14,4); }
    else if(flags&2) { st=MULIB_AUDIO_UNSUPPORTED; goto done; }
    if(mulib_minivorbis_le_(p+at+14,4)!=serial) { st=MULIB_AUDIO_UNSUPPORTED; goto done; }
    if(mulib_minivorbis_le_(p+at+18,4)!=seq++ || !!(flags&1)!=!!packet.size) goto done;
    nseg=p[at+26]; if(nseg>size-at-27) goto done;
    length=27+nseg;
    for(i=0;i<nseg;++i) length+=p[at+27+i];
    if(length>size-at || mulib_minivorbis_crc_(p+at,length)!=mulib_minivorbis_le_(p+at+22,4)) goto done;
    granule=mulib_minivorbis_le_(p+at+6,8); payload=at+27+nseg;
    for(i=0;i<nseg;++i) {
      size_t n=p[at+27+i];
      if(packet.size>1048576-n) { st=MULIB_AUDIO_LIMIT; goto done; }
      st=mulib_audio_append(&packet,p+payload,n); if(st) goto done; payload+=n;
      if(n<255) { st=mulib_minivorbis_packet_(&d,packet.data,packet.size); if(st) goto done; packet.size=0; completed=1; }
    }
    st=MULIB_AUDIO_FORMAT;
    if(completed) {
      if(granule==UINT64_MAX || granule<last_granule || granule>d.frames) goto done;
      if(!(flags&4) && granule!=d.frames) goto done;
      last_granule=granule;
    } else if(granule!=UINT64_MAX) goto done;
    eos=!!(flags&4); at+=length;
  }
  if(!eos || packet.size || d.headers!=3 || !d.blocks) goto done;
  if(last_granule>(opt&&opt->max_frames?opt->max_frames:16777216)) { st=MULIB_AUDIO_LIMIT; goto done; }
  out->samples=(float *)d.pcm.data; out->frames=(size_t)last_granule; out->rate=d.rate; out->channels=d.channels;
  memset(&d.pcm,0,sizeof(d.pcm)); st=MULIB_AUDIO_OK;
done:
  mulib_audio_bytes_free(&d.pcm); mulib_audio_bytes_free(&packet); free(d.matrix); return st;
}
#endif
