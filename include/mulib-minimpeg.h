/** @file mulib-minimpeg.h
 * @brief Original MPEG-1 Layer III audio and optional MPEG-1 video codecs.
 */
#ifndef MULIB_MINIMPEG_H
#define MULIB_MINIMPEG_H
#include "mulib-audio.h"
#ifdef __cplusplus
extern "C" {
#endif
/** Encode MP3 (not MPEG video): MPEG-1 Layer III, 320 kb/s, mono/stereo,
 * 32000/44100/48000 Hz, long blocks, Huffman books 0/1, no bit reservoir.
 * A simple binary spectral quantizer favors implementation size over fidelity;
 * this is not a psychoacoustic production encoder. Output includes filterbank
 * delay and whole-frame zero padding; no gapless tag is written.
 * SIMD accelerates analysis/MDCT. Output must be empty. */
mulib_audio_status_t mulib_minimpeg_encode (const mulib_audio_pcm_t *,
    const mulib_audio_options_t *, mulib_audio_bytes_t *);
/** Decode MPEG-1 Layer III long blocks using books 0/1, no count1 region,
 * reservoir, CRC or intensity stereo. Other features return UNSUPPORTED.
 * This covers encoder output but NOT arbitrary commercial MP3 files.
 * ID3v2/v1 tags are skipped. SIMD accelerates synthesis/IMDCT.
 * Output must be empty; failures preserve it. */
mulib_audio_status_t mulib_minimpeg_decode (const void *, size_t,
    const mulib_audio_options_t *, mulib_audio_pcm_t *);

/** Video works on a complete raw MPEG-1 elementary stream, not a container.
 * Define MULIB_MINIMPEG_USE_LIBAVCODEC in the MULIB_IMPL_MINIMPEG translation
 * unit to opt in; without it the video entry points report UNAVAILABLE. */
typedef enum mulib_minimpeg_video_status {
  MULIB_MINIMPEG_VIDEO_OK, MULIB_MINIMPEG_VIDEO_INVALID,
  MULIB_MINIMPEG_VIDEO_FORMAT, MULIB_MINIMPEG_VIDEO_UNSUPPORTED,
  MULIB_MINIMPEG_VIDEO_LIMIT, MULIB_MINIMPEG_VIDEO_MEMORY,
  MULIB_MINIMPEG_VIDEO_UNAVAILABLE, MULIB_MINIMPEG_VIDEO_BACKEND
} mulib_minimpeg_video_status_t;
enum {
  MULIB_MINIMPEG_VIDEO_ENCODER = 1, MULIB_MINIMPEG_VIDEO_DECODER = 2
};
/** Fixed, even dimensions (up to 768 x 576), with an exact MPEG-1 frame rate. */
typedef struct mulib_minimpeg_video_format {
  unsigned width, height, fps_num, fps_den;
} mulib_minimpeg_video_format_t;
/** Borrowed 8-bit Y, Cb, Cr planes in that order, top row first.
 * Stride and accessible size are in bytes; Cb/Cr have half width and height. */
typedef struct mulib_minimpeg_video_plane {
  const unsigned char *data;
  size_t stride, size;
} mulib_minimpeg_video_plane_t;
typedef struct mulib_minimpeg_video_frame {
  mulib_minimpeg_video_plane_t planes[3];
} mulib_minimpeg_video_frame_t;
/** Zero for defaults: 4096 pictures, 64 MiB compressed, 64 MiB raw. */
typedef struct mulib_minimpeg_video_limits {
  size_t max_frames, max_stream_bytes, max_raw_bytes;
} mulib_minimpeg_video_limits_t;
/** Owning output: display-order frames, each packed as tight Y, Cb, Cr.
 * Zero-initialize before decode; release with mulib_minimpeg_video_free. */
typedef struct mulib_minimpeg_video_clip {
  mulib_minimpeg_video_format_t format;
  unsigned char *data;
  size_t frames, frame_size, size;
} mulib_minimpeg_video_clip_t;
/** Bitmask of directions available in the linked software backend. */
unsigned mulib_minimpeg_video_available (void);
/** Encode a nonempty array of frame views. Output must be zero initialized;
 * free it with mulib_audio_bytes_free. Failure leaves output unchanged. */
mulib_minimpeg_video_status_t mulib_minimpeg_video_encode (
    const mulib_minimpeg_video_format_t *, const mulib_minimpeg_video_frame_t *,
    size_t frame_count, const mulib_minimpeg_video_limits_t *, mulib_audio_bytes_t *);
/** Decode one fixed-format progressive I/P/B stream. Output must be zero
 * initialized; failure leaves output unchanged. */
mulib_minimpeg_video_status_t mulib_minimpeg_video_decode (
    const void *, size_t, const mulib_minimpeg_video_limits_t *, mulib_minimpeg_video_clip_t *);
/** Free and zero an owned decoded clip; NULL safe. */
void mulib_minimpeg_video_free (mulib_minimpeg_video_clip_t *);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_MINIMPEG) && !defined(MULIB_MINIMPEG_IMPLEMENTED)
#define MULIB_MINIMPEG_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
/* ISO/IEC 11172-3, Table B.3 synthesis prototype, in units of 1/65536.
 * The standardized numeric coefficients are not a third-party codec engine.
 * See the manual for the table reference and sign/symmetry convention. */
static const int mulib_minimpeg_window_[257]={
  0,-1,-1,-1,-1,-1,-1,-2,-2,-2,-2,-3,-3,-4,-4,-5,-5,-6,-7,-7,-8,-9,-10,-11,-13,-14,-16,-17,-19,-21,-24,-26,
  -29,-31,-35,-38,-41,-45,-49,-53,-58,-63,-68,-73,-79,-85,-91,-97,-104,-111,-117,-125,-132,-139,-147,-154,-161,-169,-176,-183,-190,-196,-202,-208,
  -213,-218,-222,-225,-227,-228,-228,-227,-224,-221,-215,-208,-200,-189,-177,-163,-146,-127,-106,-83,-57,-29,2,36,72,111,153,197,244,294,347,401,
  459,519,581,645,711,779,848,919,991,1064,1137,1210,1283,1356,1428,1498,1567,1634,1698,1759,1817,1870,1919,1962,2001,2032,2057,2075,2085,2087,2080,2063,
  2037,2000,1952,1893,1822,1739,1644,1535,1414,1280,1131,970,794,605,402,185,-45,-288,-545,-814,-1095,-1388,-1692,-2006,-2330,-2663,-3004,-3351,-3705,-4063,-4425,-4788,
  -5153,-5517,-5879,-6237,-6589,-6935,-7271,-7597,-7910,-8209,-8491,-8755,-8998,-9219,-9416,-9585,-9727,-9838,-9916,-9959,-9966,-9935,-9863,-9750,-9592,-9389,-9139,-8840,-8492,-8092,-7640,-7134,
  -6574,-5959,-5288,-4561,-3776,-2935,-2037,-1082,-70,998,2122,3300,4533,5818,7154,8540,9975,11455,12980,14548,16155,17799,19478,21189,22929,24694,26482,28289,30112,31947,33791,35640,
  37489,39336,41176,43006,44821,46617,48390,50137,51853,53534,55178,56778,58333,59838,61289,62684,64019,65290,66494,67629,68692,69679,70590,71420,72169,72835,73415,73908,74313,74630,74856,74992,75038
};
static const unsigned mulib_minimpeg_bands_[3][23]={
  {0,4,8,12,16,20,24,30,36,44,52,62,74,90,110,134,162,196,238,288,342,418,576},
  {0,4,8,12,16,20,24,30,36,42,50,60,72,88,106,128,156,190,230,276,330,384,576},
  {0,4,8,12,16,20,24,30,36,44,54,66,82,102,126,156,194,240,296,364,448,550,576}
};
typedef struct mulib_minimpeg_bits_ { unsigned char *p; size_t n, at; int bad; } mulib_minimpeg_bits_;
static unsigned mulib_minimpeg_get_ (mulib_minimpeg_bits_ *b, unsigned n)
{
  unsigned v=0,i; if(b->at>b->n || n>b->n-b->at) { b->bad=1; return 0; }
  for(i=0;i<n;++i,++b->at) v=(v<<1)|((b->p[b->at/8]>>(7-b->at%8))&1);
  return v;
}
static void mulib_minimpeg_put_ (mulib_minimpeg_bits_ *b, unsigned v, unsigned n)
{
  unsigned i; if(b->at>b->n || n>b->n-b->at) { b->bad=1; return; }
  for(i=0;i<n;++i,++b->at) if((v>>(n-1-i))&1) b->p[b->at/8]|=(unsigned char)(1u<<(7-b->at%8));
}
typedef struct mulib_minimpeg_transform_ {
  float window[512], synthesis[64][32], analysis[32][64], mdct[18][36], imdct[36][18];
  float history[2][1024], overlap[2][32][18];
} mulib_minimpeg_transform_;
static mulib_minimpeg_transform_ *mulib_minimpeg_transform_create_ (void)
{
  mulib_minimpeg_transform_ *t=(mulib_minimpeg_transform_ *)calloc(1,sizeof(*t)); unsigned i,k;
  if(!t) return NULL;
  for(i=0;i<512;++i) t->window[i]=(float)(mulib_minimpeg_window_[i<=256?i:512-i]*((i/64)&1?-1:1)/65536.0);
  for(i=0;i<64;++i) for(k=0;k<32;++k) {
    t->synthesis[i][k]=(float)cos(3.14159265358979323846/64*(16+i)*(2*k+1));
    t->analysis[k][i]=(float)cos(3.14159265358979323846/64*((int)i-16)*(2*k+1));
  }
  for(i=0;i<36;++i) for(k=0;k<18;++k) {
    float v=(float)(sin(3.14159265358979323846/36*(i+0.5))*cos(3.14159265358979323846/72*(2*i+19)*(2*k+1)));
    t->mdct[k][i]=v/9; t->imdct[i][k]=v;
  }
  return t;
}
static void mulib_minimpeg_alias_ (float *x, int inverse)
{
  static const double c[8]={-0.6,-0.535,-0.33,-0.185,-0.095,-0.041,-0.0142,-0.0037}; unsigned sb,i;
  for(sb=1;sb<32;++sb) for(i=0;i<8;++i) {
    double cs=1/sqrt(1+c[i]*c[i]), ca=c[i]*cs, a=x[sb*18-1-i], b=x[sb*18+i];
    if(inverse) ca=-ca;
    x[sb*18-1-i]=(float)(a*cs-b*ca); x[sb*18+i]=(float)(b*cs+a*ca);
  }
}
static void mulib_minimpeg_synth_ (mulib_minimpeg_transform_ *t, unsigned ch, float *x, float *out, unsigned channels, mulib_xsimd_backend_t simd)
{
  float sub[32][18]; unsigned sb,i,k,j; mulib_minimpeg_alias_(x,0);
  for(sb=0;sb<32;++sb) for(i=0;i<36;++i) {
    float v; (void)mulib_xsimd_dot(simd,t->imdct[i],x+sb*18,18,&v);
    if(i<18) { sub[sb][i]=v+t->overlap[ch][sb][i]; if((sb&i&1)!=0) sub[sb][i]=-sub[sb][i]; }
    else t->overlap[ch][sb][i-18]=v;
  }
  for(i=0;i<18;++i) {
    float s[32]; for(sb=0;sb<32;++sb) s[sb]=sub[sb][i];
    memmove(t->history[ch]+64,t->history[ch],960*sizeof(float));
    for(k=0;k<64;++k) (void)mulib_xsimd_dot(simd,s,t->synthesis[k],32,t->history[ch]+k);
    for(j=0;j<32;++j) {
      double sum=0;
      for(k=0;k<16;++k) { unsigned index=k*32+j, vindex=(k/2)*128+j+(k&1?96:0); sum+=t->history[ch][vindex]*t->window[index]; }
      out[(i*32+j)*channels+ch]=(float)sum;
    }
  }
}
static void mulib_minimpeg_analyze_ (mulib_minimpeg_transform_ *t, const mulib_audio_pcm_t *p,
    size_t start, unsigned ch, float *spectrum, mulib_xsimd_backend_t simd)
{
  float sub[32][36]; unsigned i,k,j,sb;
  for(sb=0;sb<32;++sb) memcpy(sub[sb],t->overlap[ch][sb],18*sizeof(float));
  for(i=0;i<18;++i) {
    float y[64]; memmove(t->history[ch]+32,t->history[ch],480*sizeof(float));
    for(j=0;j<32;++j) {
      size_t pos=start+i*32+j; float v=pos<p->frames?p->samples[pos*p->channels+ch]:0;
      t->history[ch][31-j]=v>1?1:v<-1?-1:v;
    }
    for(j=0;j<64;++j) { double v=0; for(k=0;k<8;++k) v+=t->history[ch][j+k*64]*t->window[j+k*64]/32; y[j]=(float)v; }
    for(sb=0;sb<32;++sb) {
      float v; (void)mulib_xsimd_dot(simd,y,t->analysis[sb],64,&v); if((sb&i&1)!=0) v=-v;
      sub[sb][18+i]=v; t->overlap[ch][sb][i]=v;
    }
  }
  for(sb=0;sb<32;++sb) for(k=0;k<18;++k) (void)mulib_xsimd_dot(simd,sub[sb],t->mdct[k],36,spectrum+sb*18+k);
  mulib_minimpeg_alias_(spectrum,1);
}
mulib_audio_status_t mulib_minimpeg_encode (const mulib_audio_pcm_t *p, const mulib_audio_options_t *opt, mulib_audio_bytes_t *out)
{
  mulib_audio_bytes_t b={0}; mulib_minimpeg_transform_ *t; mulib_audio_status_t st; size_t f, frames, length;
  unsigned ri, side; mulib_xsimd_backend_t simd=opt?opt->simd:MULIB_XSIMD_AUTO;
  if(!out || out->data || out->size || out->capacity) return MULIB_AUDIO_INVALID;
  st=mulib_audio_validate(p,opt); if(st) return st;
  if(p->channels>2 || (p->rate!=44100 && p->rate!=48000 && p->rate!=32000)) return MULIB_AUDIO_UNSUPPORTED;
  if(p->frames>SIZE_MAX-2303) return MULIB_AUDIO_LIMIT;
  ri=p->rate==44100?0:p->rate==48000?1:2; side=p->channels==1?17:32; length=144*320000/p->rate;
  frames=(p->frames+2303)/1152; t=mulib_minimpeg_transform_create_(); if(!t) return MULIB_AUDIO_MEMORY;
  for(f=0;!st && f<frames;++f) {
    unsigned char packet[1440]={0}; mulib_minimpeg_bits_ info, main; unsigned gr,ch;
    packet[0]=255; packet[1]=251; packet[2]=(unsigned char)(0xe0|(ri<<2)); packet[3]=(unsigned char)(p->channels==1?0xc0:0);
    info.p=packet+4; info.n=side*8; info.at=0; info.bad=0;
    main.p=packet+4+side; main.n=(length-4-side)*8; main.at=0; main.bad=0;
    mulib_minimpeg_put_(&info,0,9); mulib_minimpeg_put_(&info,0,p->channels==1?5:3);
    for(ch=0;ch<p->channels;++ch) mulib_minimpeg_put_(&info,0,4);
    for(gr=0;gr<2;++gr) for(ch=0;ch<p->channels;++ch) {
      float x[576]; unsigned sf[22]={0}, gg,k,band; int q[576]; double peak=0,gain; size_t begin=main.at;
      mulib_minimpeg_analyze_(t,p,f*1152+gr*576,ch,x,simd);
      for(k=0;k<576;++k) if(fabs(x[k])>peak) peak=fabs(x[k]);
      { double v=peak>0?ceil(210+4*log2(peak)):0; gg=(unsigned)(v<0?0:v>255?255:v); }
      gain=exp2(((int)gg-210)/4.0);
      for(band=0;band<22;++band) {
        double bp=0, scale;
        for(k=mulib_minimpeg_bands_[ri][band];k<mulib_minimpeg_bands_[ri][band+1];++k) if(fabs(x[k])>bp) bp=fabs(x[k]);
        if(band<21 && bp>0) { double v=floor(2*log2(gain/bp)); unsigned max=band<11?15:7; sf[band]=(unsigned)(v<0?0:v>max?max:v); }
        scale=gain*exp2(-0.5*sf[band]);
        for(k=mulib_minimpeg_bands_[ri][band];k<mulib_minimpeg_bands_[ri][band+1];++k) q[k]=fabs(x[k])<scale*0.45?0:x[k]<0?-1:1;
      }
      for(band=0;band<21;++band) mulib_minimpeg_put_(&main,sf[band],band<11?4:3);
      for(k=0;k<576;k+=2) {
        unsigned a=q[k]!=0, c=q[k+1]!=0;
        if(!a&&!c) mulib_minimpeg_put_(&main,1,1);
        else if(a&&!c) mulib_minimpeg_put_(&main,1,2);
        else mulib_minimpeg_put_(&main,a?0:1,3);
        if(a) mulib_minimpeg_put_(&main,q[k]<0,1);
        if(c) mulib_minimpeg_put_(&main,q[k+1]<0,1);
      }
      mulib_minimpeg_put_(&info,(unsigned)(main.at-begin),12); mulib_minimpeg_put_(&info,288,9); mulib_minimpeg_put_(&info,gg,8);
      mulib_minimpeg_put_(&info,15,4); mulib_minimpeg_put_(&info,0,1);
      for(k=0;k<3;++k) mulib_minimpeg_put_(&info,1,5);
      mulib_minimpeg_put_(&info,7,4); mulib_minimpeg_put_(&info,5,3); mulib_minimpeg_put_(&info,0,3);
    }
    if(info.bad || main.bad) st=MULIB_AUDIO_LIMIT; else st=mulib_audio_append(&b,packet,length);
  }
  free(t); if(st) mulib_audio_bytes_free(&b); else *out=b; return st;
}
typedef struct mulib_minimpeg_granule_ { unsigned length,big,gain,compress,table[3],r0,r1,pre,scale,count; } mulib_minimpeg_granule_;
mulib_audio_status_t mulib_minimpeg_decode (const void *data, size_t size, const mulib_audio_options_t *opt, mulib_audio_pcm_t *out)
{
  const unsigned char *p=(const unsigned char *)data; size_t at=0,frames=0,max=opt&&opt->max_frames?opt->max_frames:16777216;
  unsigned rate=0, channels=0; mulib_audio_bytes_t pcm={0}; mulib_minimpeg_transform_ *t;
  mulib_audio_status_t st=MULIB_AUDIO_FORMAT; mulib_xsimd_backend_t simd=opt?opt->simd:MULIB_XSIMD_AUTO;
  if(!out || out->samples || out->frames || (!data && size)) return MULIB_AUDIO_INVALID;
  if(!mulib_xsimd_available(simd)) return MULIB_AUDIO_UNSUPPORTED;
  if(size>=10 && !memcmp(p,"ID3",3)) {
    unsigned i; size_t n=0;
    if(p[3]<2 || p[3]>4) return MULIB_AUDIO_UNSUPPORTED;
    for(i=6;i<10;++i) { if(p[i]&128) return MULIB_AUDIO_FORMAT; n=(n<<7)|p[i]; }
    if(n>size-10) return MULIB_AUDIO_FORMAT;
    at=10+n; if(p[3]==4 && (p[5]&16)) { if(size-at<10) return MULIB_AUDIO_FORMAT; at+=10; }
  }
  if(size-at>=128 && !memcmp(p+size-128,"TAG",3)) size-=128;
  t=mulib_minimpeg_transform_create_(); if(!t) return MULIB_AUDIO_MEMORY;
  while(at<size) {
    static const unsigned rates[3]={44100,48000,32000}, kbps[15]={0,32,40,48,56,64,80,96,112,128,160,192,224,256,320};
    unsigned ri,br,ch,mode,ext,side,gr,c,i,scfsi[2][4]={{0}}, sf[2][22]={{0}};
    size_t length; mulib_minimpeg_bits_ info,main; mulib_minimpeg_granule_ g[2][2]; float block[2304];
    if(size-at<4 || p[at]!=255 || (p[at+1]&224)!=224) goto done;
    if((p[at+1]&30)!=26 || !(p[at+1]&1)) { st=MULIB_AUDIO_UNSUPPORTED; goto done; }
    ri=(p[at+2]>>2)&3; br=p[at+2]>>4; mode=p[at+3]>>6; ext=(p[at+3]>>4)&3;
    if(ri==3 || br==15 || (p[at+3]&3)==2) goto done;
    if(!br || (p[at+3]&3) || (mode==1 && (ext&1))) { st=MULIB_AUDIO_UNSUPPORTED; goto done; }
    ch=mode==3?1:2; side=ch==1?17:32; length=144000*kbps[br]/rates[ri]+((p[at+2]>>1)&1);
    if(length<4+side || length>size-at) goto done;
    if(rate && (rate!=rates[ri] || channels!=ch)) { st=MULIB_AUDIO_UNSUPPORTED; goto done; }
    rate=rates[ri]; channels=ch;
    if(frames>max || 1152>max-frames) { st=MULIB_AUDIO_LIMIT; goto done; }
    info.p=(unsigned char *)(p+at+4); info.n=side*8; info.at=0; info.bad=0;
    if(mulib_minimpeg_get_(&info,9)) { st=MULIB_AUDIO_UNSUPPORTED; goto done; }
    (void)mulib_minimpeg_get_(&info,ch==1?5:3);
    for(c=0;c<ch;++c) for(i=0;i<4;++i) scfsi[c][i]=mulib_minimpeg_get_(&info,1);
    for(gr=0;gr<2;++gr) for(c=0;c<ch;++c) {
      mulib_minimpeg_granule_ *v=&g[gr][c];
      v->length=mulib_minimpeg_get_(&info,12); v->big=mulib_minimpeg_get_(&info,9); v->gain=mulib_minimpeg_get_(&info,8); v->compress=mulib_minimpeg_get_(&info,4);
      if(mulib_minimpeg_get_(&info,1)) { st=MULIB_AUDIO_UNSUPPORTED; goto done; }
      for(i=0;i<3;++i) { v->table[i]=mulib_minimpeg_get_(&info,5); if(v->table[i]>1) { st=MULIB_AUDIO_UNSUPPORTED; goto done; } }
      v->r0=mulib_minimpeg_get_(&info,4); v->r1=mulib_minimpeg_get_(&info,3);
      v->pre=mulib_minimpeg_get_(&info,1); v->scale=mulib_minimpeg_get_(&info,1); v->count=mulib_minimpeg_get_(&info,1);
      if(v->big>288 || v->r0+v->r1+2>22 || info.bad) goto done;
    }
    main.p=(unsigned char *)(p+at+4+side); main.n=(length-4-side)*8; main.at=0; main.bad=0;
    for(gr=0;gr<2;++gr) {
      float x[2][576]={{0}};
      for(c=0;c<ch;++c) {
        static const unsigned sl1[16]={0,0,0,0,3,1,1,1,2,2,2,3,3,3,4,4}, sl2[16]={0,1,2,3,0,1,2,3,1,2,3,1,2,3,2,3};
        static const unsigned pre[22]={0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,2,2,3,3,3,2,0};
        mulib_minimpeg_granule_ *v=&g[gr][c]; size_t end=main.at+v->length; unsigned band=0;
        if(end>main.n) goto done;
        for(i=0;i<21;++i) { unsigned group=i<6?0:i<11?1:i<16?2:3; if(!gr || !scfsi[c][group]) sf[c][i]=mulib_minimpeg_get_(&main,i<11?sl1[v->compress]:sl2[v->compress]); }
        for(i=0;i<v->big*2;i+=2) {
          unsigned table=i<mulib_minimpeg_bands_[ri][v->r0+1]?v->table[0]:i<mulib_minimpeg_bands_[ri][v->r0+v->r1+2]?v->table[1]:v->table[2], a=0,b=0;
          if(table && !mulib_minimpeg_get_(&main,1)) {
            if(mulib_minimpeg_get_(&main,1)) a=1;
            else { b=1; a=!mulib_minimpeg_get_(&main,1); }
          }
          x[c][i]=a?(mulib_minimpeg_get_(&main,1)?-1.0f:1.0f):0;
          x[c][i+1]=b?(mulib_minimpeg_get_(&main,1)?-1.0f:1.0f):0;
        }
        if(main.bad || main.at>end) goto done;
        if(main.at!=end) { st=MULIB_AUDIO_UNSUPPORTED; goto done; }
        for(i=0;i<576;++i) {
          while(i>=mulib_minimpeg_bands_[ri][band+1]) ++band;
          x[c][i]*=(float)exp2(((int)v->gain-210)/4.0-(v->scale?1:0.5)*(sf[c][band]+v->pre*pre[band]));
        }
      }
      if(mode==1 && (ext&2)) for(i=0;i<576;++i) { float m=x[0][i],s=x[1][i]; x[0][i]=(m+s)*0.7071067811865475f; x[1][i]=(m-s)*0.7071067811865475f; }
      for(c=0;c<ch;++c) mulib_minimpeg_synth_(t,c,x[c],block+gr*576*ch,ch,simd);
    }
    st=mulib_audio_append(&pcm,block,1152*ch*sizeof(float)); if(st) goto done;
    frames+=1152; at+=length; st=MULIB_AUDIO_FORMAT;
  }
  if(!frames) goto done;
  out->samples=(float *)pcm.data; out->frames=frames; out->rate=rate; out->channels=channels;
  memset(&pcm,0,sizeof(pcm)); st=MULIB_AUDIO_OK;
done:
  free(t); mulib_audio_bytes_free(&pcm); return st;
}

void mulib_minimpeg_video_free (mulib_minimpeg_video_clip_t *clip)
{
  if(clip) { free(clip->data); memset(clip,0,sizeof(*clip)); }
}

#ifdef MULIB_MINIMPEG_USE_LIBAVCODEC
#include "mulib-codec-core.h"
#include <errno.h>
#include <limits.h>
#include <libavcodec/avcodec.h>
#include <libavutil/error.h>

#if LIBAVCODEC_VERSION_MAJOR < 59
#error "MULIB_MINIMPEG_USE_LIBAVCODEC requires libavcodec 59 (FFmpeg 5) or newer"
#endif

enum { MULIB_MINIMPEG_VIDEO_MAX_WIDTH_ = 768, MULIB_MINIMPEG_VIDEO_MAX_HEIGHT_ = 576 };
static const unsigned mulib_minimpeg_video_rates_[8][2]={
  {24000,1001},{24,1},{25,1},{30000,1001},{30,1},{50,1},{60000,1001},{60,1}
};

static mulib_minimpeg_video_limits_t mulib_minimpeg_video_limits_ (
    const mulib_minimpeg_video_limits_t *limits)
{
  mulib_minimpeg_video_limits_t l;
  l.max_frames=limits && limits->max_frames ? limits->max_frames : 4096;
  l.max_stream_bytes=limits && limits->max_stream_bytes ? limits->max_stream_bytes : 64UL*1024*1024;
  /* The native parser takes a signed int length and may add SIMD padding. */
  if(l.max_stream_bytes>INT_MAX-AV_INPUT_BUFFER_PADDING_SIZE)
    l.max_stream_bytes=INT_MAX-AV_INPUT_BUFFER_PADDING_SIZE;
  l.max_raw_bytes=limits && limits->max_raw_bytes ? limits->max_raw_bytes : 64UL*1024*1024;
  return l;
}

static mulib_minimpeg_video_status_t mulib_minimpeg_video_shape_ (
    const mulib_minimpeg_video_format_t *f, size_t *bytes)
{
  if(!f->width || !f->height || !f->fps_num || !f->fps_den)
    return MULIB_MINIMPEG_VIDEO_INVALID;
  if(f->width>MULIB_MINIMPEG_VIDEO_MAX_WIDTH_ || f->height>MULIB_MINIMPEG_VIDEO_MAX_HEIGHT_)
    return MULIB_MINIMPEG_VIDEO_LIMIT;
  if((f->width | f->height)&1) return MULIB_MINIMPEG_VIDEO_UNSUPPORTED;
  *bytes=(size_t)f->width*f->height*3/2;
  return MULIB_MINIMPEG_VIDEO_OK;
}

static mulib_minimpeg_video_status_t mulib_minimpeg_video_averror_ (int rc)
{
  if(rc==AVERROR(ENOMEM)) return MULIB_MINIMPEG_VIDEO_MEMORY;
  if(rc==AVERROR_INVALIDDATA) return MULIB_MINIMPEG_VIDEO_FORMAT;
  return MULIB_MINIMPEG_VIDEO_BACKEND;
}

static int mulib_minimpeg_video_has_parser_ (void)
{
  void *it=NULL;
  const AVCodecParser *parser;
  while((parser=av_parser_iterate(&it))) {
    size_t i;
    for(i=0;i<sizeof(parser->codec_ids)/sizeof(parser->codec_ids[0]);++i)
      if(parser->codec_ids[i]==AV_CODEC_ID_MPEG1VIDEO) return 1;
  }
  return 0;
}

unsigned mulib_minimpeg_video_available (void)
{
  return (avcodec_find_encoder_by_name("mpeg1video") ? MULIB_MINIMPEG_VIDEO_ENCODER : 0u) |
      (avcodec_find_decoder_by_name("mpeg1video") && mulib_minimpeg_video_has_parser_() ?
          MULIB_MINIMPEG_VIDEO_DECODER : 0u);
}

static mulib_minimpeg_video_status_t mulib_minimpeg_video_append_ (
    mulib_buffer_t *b, const void *p, size_t n, size_t limit)
{
  switch(mulib_codec_append(b,p,n,limit)) {
  case MULIB_CODEC_OK: return MULIB_MINIMPEG_VIDEO_OK;
  case MULIB_CODEC_ELIMIT: return MULIB_MINIMPEG_VIDEO_LIMIT;
  case MULIB_CODEC_ENOMEM: return MULIB_MINIMPEG_VIDEO_MEMORY;
  default: return MULIB_MINIMPEG_VIDEO_BACKEND;
  }
}

/* Drain after each send, including the final NULL frame. */
static mulib_minimpeg_video_status_t mulib_minimpeg_video_packets_ (
    AVCodecContext *ctx, AVPacket *packet, const AVFrame *frame,
    mulib_buffer_t *b, size_t limit)
{
  int rc=avcodec_send_frame(ctx,frame);
  if(rc<0) return mulib_minimpeg_video_averror_(rc);
  for(;;) {
    mulib_minimpeg_video_status_t st;
    rc=avcodec_receive_packet(ctx,packet);
    if(rc==(frame ? AVERROR(EAGAIN) : AVERROR_EOF)) return MULIB_MINIMPEG_VIDEO_OK;
    if(rc<0) return mulib_minimpeg_video_averror_(rc);
    st=mulib_minimpeg_video_append_(b,packet->data,(size_t)packet->size,limit);
    av_packet_unref(packet);
    if(st) return st;
  }
}

mulib_minimpeg_video_status_t mulib_minimpeg_video_encode (
    const mulib_minimpeg_video_format_t *f, const mulib_minimpeg_video_frame_t *frames,
    size_t count, const mulib_minimpeg_video_limits_t *limits, mulib_audio_bytes_t *out)
{
  static const unsigned char end[]={0,0,1,0xb7};
  mulib_minimpeg_video_limits_t l=mulib_minimpeg_video_limits_(limits);
  mulib_minimpeg_video_status_t st;
  mulib_buffer_t b={0};
  const AVCodec *codec;
  AVCodecContext *ctx=NULL;
  AVFrame *frame=NULL;
  AVPacket *packet=NULL;
  size_t frame_bytes, i;
  unsigned p, rate;
  int rc;
  if(!f || !frames || !count || !out || out->data || out->size || out->capacity)
    return MULIB_MINIMPEG_VIDEO_INVALID;
  st=mulib_minimpeg_video_shape_(f,&frame_bytes);
  if(st) return st;
  for(rate=0;rate<8;++rate) {
    if((uint64_t)f->fps_num*mulib_minimpeg_video_rates_[rate][1]==
        (uint64_t)f->fps_den*mulib_minimpeg_video_rates_[rate][0]) break;
  }
  if(rate==8) return MULIB_MINIMPEG_VIDEO_UNSUPPORTED;
  if(count>l.max_frames || count>l.max_raw_bytes/frame_bytes ||
      count>SIZE_MAX/sizeof(*frames) || count>(uint64_t)INT64_MAX)
    return MULIB_MINIMPEG_VIDEO_LIMIT;
  for(i=0;i<count;++i) for(p=0;p<3;++p) {
    const mulib_minimpeg_video_plane_t *src=&frames[i].planes[p];
    unsigned w=f->width>>(p!=0), h=f->height>>(p!=0);
    if(!src->data || src->stride<w || src->size<w ||
        (h>1 && src->stride>(src->size-w)/(h-1))) return MULIB_MINIMPEG_VIDEO_INVALID;
  }
  codec=avcodec_find_encoder_by_name("mpeg1video");
  if(!codec) return MULIB_MINIMPEG_VIDEO_UNAVAILABLE;
  ctx=avcodec_alloc_context3(codec);
  frame=av_frame_alloc();
  packet=av_packet_alloc();
  if(!ctx || !frame || !packet) { st=MULIB_MINIMPEG_VIDEO_MEMORY; goto done; }
  ctx->width=(int)f->width;
  ctx->height=(int)f->height;
  ctx->pix_fmt=AV_PIX_FMT_YUV420P;
  ctx->color_range=AVCOL_RANGE_MPEG;
  ctx->framerate.num=(int)mulib_minimpeg_video_rates_[rate][0];
  ctx->framerate.den=(int)mulib_minimpeg_video_rates_[rate][1];
  ctx->time_base.num=ctx->framerate.den;
  ctx->time_base.den=ctx->framerate.num;
  ctx->sample_aspect_ratio.num=ctx->sample_aspect_ratio.den=1;
  ctx->thread_count=1;
  ctx->flags|=AV_CODEC_FLAG_QSCALE | AV_CODEC_FLAG_BITEXACT;
  ctx->gop_size=12;
  ctx->max_b_frames=0;
  rc=avcodec_open2(ctx,codec,NULL);
  if(rc<0) { st=mulib_minimpeg_video_averror_(rc); goto done; }
  frame->format=ctx->pix_fmt;
  frame->width=ctx->width;
  frame->height=ctx->height;
  frame->quality=3*FF_QP2LAMBDA;
  rc=av_frame_get_buffer(frame,32);
  if(rc<0) { st=mulib_minimpeg_video_averror_(rc); goto done; }
  for(i=0;i<count;++i) {
    rc=av_frame_make_writable(frame);
    if(rc<0) { st=mulib_minimpeg_video_averror_(rc); goto done; }
    frame->pts=(int64_t)i;
    for(p=0;p<3;++p) {
      const mulib_minimpeg_video_plane_t *src=&frames[i].planes[p];
      unsigned w=f->width>>(p!=0), h=f->height>>(p!=0), y;
      for(y=0;y<h;++y)
        memcpy(frame->data[p]+y*(size_t)frame->linesize[p],src->data+y*src->stride,w);
    }
    st=mulib_minimpeg_video_packets_(ctx,packet,frame,&b,l.max_stream_bytes);
    if(st) goto done;
  }
  st=mulib_minimpeg_video_packets_(ctx,packet,NULL,&b,l.max_stream_bytes);
  if(st) goto done;
  if(!b.length) { st=MULIB_MINIMPEG_VIDEO_BACKEND; goto done; }
  if(b.length<sizeof(end) || memcmp(b.data+b.length-sizeof(end),end,sizeof(end)))
    st=mulib_minimpeg_video_append_(&b,end,sizeof(end),l.max_stream_bytes);
  if(!st) { out->data=b.data; out->size=b.length; out->capacity=b.capacity; b.data=NULL; }
done:
  free(b.data);
  av_packet_free(&packet);
  av_frame_free(&frame);
  avcodec_free_context(&ctx);
  return st;
}

static size_t mulib_minimpeg_video_start_ (const unsigned char *p, size_t n, size_t at)
{
  while(n-at>=3) {
    if(!p[at] && !p[at+1] && p[at+2]==1) return at;
    ++at;
  }
  return n;
}

static int mulib_minimpeg_video_sequence_ (mulib_codec_bits_t *b, mulib_minimpeg_video_format_t *f)
{
  uint32_t w,h,aspect,rate,bitrate,marker,v,load;
  unsigned m,i;
  if(!mulib_codec_get(b,12,&w) || !mulib_codec_get(b,12,&h) ||
      !mulib_codec_get(b,4,&aspect) || !mulib_codec_get(b,4,&rate) ||
      !mulib_codec_get(b,18,&bitrate) || !mulib_codec_get(b,1,&marker) ||
      !mulib_codec_get(b,11,&v) || !w || !h || !aspect || aspect>=15 ||
      !rate || rate>8 || !bitrate || !marker) return 0;
  for(m=0;m<2;++m) {
    if(!mulib_codec_get(b,1,&load)) return 0;
    if(load) for(i=0;i<64;++i) if(!mulib_codec_get(b,8,&v) || !v) return 0;
  }
  f->width=w;
  f->height=h;
  f->fps_num=mulib_minimpeg_video_rates_[rate-1][0];
  f->fps_den=mulib_minimpeg_video_rates_[rate-1][1];
  return 1;
}

/* Validate the envelope before exposing it to the codec. The native parser
 * and decoder still own packet boundaries, entropy decoding and reordering. */
static mulib_minimpeg_video_status_t mulib_minimpeg_video_scan_ (
    const unsigned char *p, size_t n, const mulib_minimpeg_video_limits_t *l,
    mulib_minimpeg_video_clip_t *clip, size_t *payload)
{
  size_t at=0, next;
  unsigned slice=0;
  int picture=0, header_pending=1;
  *payload=n;
  if(n<4 || p[0] || p[1] || p[2]!=1 || p[3]!=0xb3) return MULIB_MINIMPEG_VIDEO_FORMAT;
  while(at<n) {
    unsigned code;
    mulib_codec_bits_t bits={0};
    uint32_t v,type;
    if(n-at<4) return MULIB_MINIMPEG_VIDEO_FORMAT;
    code=p[at+3];
    next=mulib_minimpeg_video_start_(p,n,at+4);
    bits.data=p+at+4;
    bits.size=next-at-4;
    if(code>=1 && code<=0xaf) {
      unsigned rows=(clip->format.height+15)/16;
      if(!picture || code<slice || code>rows || bits.size<2 ||
          !mulib_codec_get(&bits,5,&v) || !v) return MULIB_MINIMPEG_VIDEO_FORMAT;
      slice=code;
    } else if(code!=0xb2) { /* user_data may sit between headers and slices */
      if(picture && !slice) return MULIB_MINIMPEG_VIDEO_FORMAT;
      picture=0;
      slice=0;
      switch(code) {
      case 0xb3: {
        mulib_minimpeg_video_format_t f;
        size_t bytes;
        mulib_minimpeg_video_status_t st;
        if(!mulib_minimpeg_video_sequence_(&bits,&f)) return MULIB_MINIMPEG_VIDEO_FORMAT;
        st=mulib_minimpeg_video_shape_(&f,&bytes);
        if(st) return st;
        if(clip->frame_size && (clip->format.width!=f.width || clip->format.height!=f.height ||
            clip->format.fps_num!=f.fps_num || clip->format.fps_den!=f.fps_den))
          return MULIB_MINIMPEG_VIDEO_UNSUPPORTED;
        clip->format=f;
        clip->frame_size=bytes;
        header_pending=1;
        break;
      }
      case 0xb8: /* group_start_code with a 27-bit time code/flags */
        if(bits.size<4) return MULIB_MINIMPEG_VIDEO_FORMAT;
        header_pending=1;
        break;
      case 0x00: /* temporal reference, picture coding type, VBV delay */
        if(!mulib_codec_get(&bits,10,&v) || !mulib_codec_get(&bits,3,&type) ||
            !mulib_codec_get(&bits,16,&v) || !type || type>=5)
          return MULIB_MINIMPEG_VIDEO_FORMAT;
        if(type==4) return MULIB_MINIMPEG_VIDEO_UNSUPPORTED;
        if(!clip->frames && type!=1) return MULIB_MINIMPEG_VIDEO_FORMAT;
        if(type>=2 && (!mulib_codec_get(&bits,4,&v) || !(v&7)))
          return MULIB_MINIMPEG_VIDEO_FORMAT;
        if(type==3 && (!mulib_codec_get(&bits,4,&v) || !(v&7)))
          return MULIB_MINIMPEG_VIDEO_FORMAT;
        do {
          if(!mulib_codec_get(&bits,1,&v)) return MULIB_MINIMPEG_VIDEO_FORMAT;
          if(!v) break;
          if(!mulib_codec_get(&bits,8,&v)) return MULIB_MINIMPEG_VIDEO_FORMAT;
        } while(1);
        if(clip->frames>=l->max_frames || clip->frames>=l->max_raw_bytes/clip->frame_size)
          return MULIB_MINIMPEG_VIDEO_LIMIT;
        ++clip->frames;
        picture=1;
        header_pending=0;
        break;
      case 0xb7: /* sequence_end_code; only zero stuffing may follow. */
        if(header_pending) return MULIB_MINIMPEG_VIDEO_FORMAT;
        *payload=at;
        for(at+=4;at<n;++at) if(p[at]) return MULIB_MINIMPEG_VIDEO_FORMAT;
        next=n;
        break;
      case 0xb5: return MULIB_MINIMPEG_VIDEO_UNSUPPORTED; /* no MPEG-2 extensions */
      default: return MULIB_MINIMPEG_VIDEO_FORMAT;
      }
    }
    at=next;
  }
  if(header_pending || (picture && !slice)) return MULIB_MINIMPEG_VIDEO_FORMAT;
  clip->size=clip->frames*clip->frame_size;
  return MULIB_MINIMPEG_VIDEO_OK;
}

static enum AVPixelFormat mulib_minimpeg_video_pixfmt_ (
    AVCodecContext *ctx, const enum AVPixelFormat *formats)
{
  (void)ctx;
  for(;*formats!=AV_PIX_FMT_NONE;++formats) if(*formats==AV_PIX_FMT_YUV420P) return *formats;
  return AV_PIX_FMT_NONE;
}

static mulib_minimpeg_video_status_t mulib_minimpeg_video_frames_ (
    AVCodecContext *ctx, AVFrame *frame, const AVPacket *pkt,
    const mulib_minimpeg_video_clip_t *clip, size_t *count)
{
  int rc=avcodec_send_packet(ctx,pkt);
  if(rc<0) return mulib_minimpeg_video_averror_(rc);
  for(;;) {
    unsigned char *dst;
    unsigned p;
    rc=avcodec_receive_frame(ctx,frame);
    if(rc==(pkt ? AVERROR(EAGAIN) : AVERROR_EOF)) return MULIB_MINIMPEG_VIDEO_OK;
    if(rc<0) return mulib_minimpeg_video_averror_(rc);
    if((frame->flags&AV_FRAME_FLAG_CORRUPT) || frame->decode_error_flags || *count>=clip->frames)
      return MULIB_MINIMPEG_VIDEO_FORMAT;
    if(frame->width!=(int)clip->format.width || frame->height!=(int)clip->format.height ||
        frame->format!=AV_PIX_FMT_YUV420P) return MULIB_MINIMPEG_VIDEO_UNSUPPORTED;
    dst=clip->data+*count*clip->frame_size;
    for(p=0;p<3;++p) {
      unsigned w=clip->format.width>>(p!=0), h=clip->format.height>>(p!=0), y;
      if(!frame->data[p] || frame->linesize[p]<(int)w) return MULIB_MINIMPEG_VIDEO_BACKEND;
      for(y=0;y<h;++y) {
        memcpy(dst,frame->data[p]+y*(size_t)frame->linesize[p],w);
        dst+=w;
      }
    }
    ++*count;
    av_frame_unref(frame);
  }
}

mulib_minimpeg_video_status_t mulib_minimpeg_video_decode (
    const void *data, size_t size, const mulib_minimpeg_video_limits_t *limits,
    mulib_minimpeg_video_clip_t *out)
{
  const unsigned char *p=(const unsigned char *)data;
  mulib_minimpeg_video_limits_t l=mulib_minimpeg_video_limits_(limits);
  mulib_minimpeg_video_clip_t clip={0};
  mulib_minimpeg_video_status_t st;
  const AVCodec *codec;
  AVCodecContext *ctx=NULL;
  AVCodecParserContext *parser=NULL;
  AVFrame *frame=NULL;
  AVPacket *packet=NULL;
  unsigned char *padded=NULL;
  size_t at=0, count=0, packets=0;
  int rc;
  if(!out || out->data || out->frames || out->frame_size || out->size ||
      out->format.width || out->format.height || out->format.fps_num || out->format.fps_den ||
      (!data && size)) return MULIB_MINIMPEG_VIDEO_INVALID;
  if(size>l.max_stream_bytes) return MULIB_MINIMPEG_VIDEO_LIMIT;
  codec=avcodec_find_decoder_by_name("mpeg1video");
  if(!codec || !mulib_minimpeg_video_has_parser_()) return MULIB_MINIMPEG_VIDEO_UNAVAILABLE;
  /* Do not feed a terminal sequence_end_code to the strict slice decoder. */
  st=mulib_minimpeg_video_scan_(p,size,&l,&clip,&size);
  if(st) return st;
  ctx=avcodec_alloc_context3(codec);
  frame=av_frame_alloc();
  packet=av_packet_alloc();
  if(!ctx || !frame || !packet) { st=MULIB_MINIMPEG_VIDEO_MEMORY; goto done; }
  parser=av_parser_init(AV_CODEC_ID_MPEG1VIDEO);
  if(!parser) { st=MULIB_MINIMPEG_VIDEO_MEMORY; goto done; }
  ctx->thread_count=1;
  ctx->get_format=mulib_minimpeg_video_pixfmt_;
  ctx->flags|=AV_CODEC_FLAG_BITEXACT;
  ctx->err_recognition=AV_EF_BITSTREAM | AV_EF_BUFFER | AV_EF_EXPLODE;
  /* Include headroom for aligned/coded surfaces; scan enforces exact geometry. */
  ctx->max_pixels=(MULIB_MINIMPEG_VIDEO_MAX_WIDTH_+128)*(MULIB_MINIMPEG_VIDEO_MAX_HEIGHT_+128);
  rc=avcodec_open2(ctx,codec,NULL);
  if(rc<0) { st=mulib_minimpeg_video_averror_(rc); goto done; }
  clip.data=(unsigned char *)malloc(clip.size);
  padded=(unsigned char *)malloc(size+AV_INPUT_BUFFER_PADDING_SIZE);
  if(!clip.data || !padded) { st=MULIB_MINIMPEG_VIDEO_MEMORY; goto done; }
  memcpy(padded,p,size);
  memset(padded+size,0,AV_INPUT_BUFFER_PADDING_SIZE);
  for(;;) {
    unsigned char *parsed=NULL;
    int parsed_size=0;
    rc=av_parser_parse2(parser,ctx,&parsed,&parsed_size,padded+at,
        (int)(size-at),AV_NOPTS_VALUE,AV_NOPTS_VALUE,(int64_t)at);
    if(rc<0) { st=mulib_minimpeg_video_averror_(rc); goto done; }
    if((size_t)rc>size-at || parsed_size<0 || (size_t)parsed_size>size || (parsed_size && !parsed)) {
      st=MULIB_MINIMPEG_VIDEO_BACKEND; goto done;
    }
    if(!rc && !parsed_size) {
      if(at==size) break;
      st=MULIB_MINIMPEG_VIDEO_FORMAT; goto done;
    }
    at+=(size_t)rc;
    if(parsed_size) {
      if(packets>=clip.frames) { st=MULIB_MINIMPEG_VIDEO_FORMAT; goto done; }
      ++packets;
      /* Parser output may alias the stream mid-buffer; give the decoder
       * a reference-counted packet with its own zero padding. */
      rc=av_new_packet(packet,parsed_size);
      if(rc<0) { st=mulib_minimpeg_video_averror_(rc); goto done; }
      memcpy(packet->data,parsed,(size_t)parsed_size);
      st=mulib_minimpeg_video_frames_(ctx,frame,packet,&clip,&count);
      av_packet_unref(packet);
      if(st) goto done;
    }
  }
  st=mulib_minimpeg_video_frames_(ctx,frame,NULL,&clip,&count);
  if(!st && count!=clip.frames) st=MULIB_MINIMPEG_VIDEO_FORMAT;
  if(!st) { *out=clip; clip.data=NULL; }
done:
  mulib_minimpeg_video_free(&clip);
  av_packet_free(&packet);
  av_frame_free(&frame);
  if(parser) av_parser_close(parser);
  avcodec_free_context(&ctx);
  free(padded);
  return st;
}

#else
unsigned mulib_minimpeg_video_available (void) { return 0; }
mulib_minimpeg_video_status_t mulib_minimpeg_video_encode (
    const mulib_minimpeg_video_format_t *f, const mulib_minimpeg_video_frame_t *frames,
    size_t count, const mulib_minimpeg_video_limits_t *limits, mulib_audio_bytes_t *out)
{
  (void)f; (void)frames; (void)count; (void)limits; (void)out;
  return MULIB_MINIMPEG_VIDEO_UNAVAILABLE;
}
mulib_minimpeg_video_status_t mulib_minimpeg_video_decode (
    const void *data, size_t size, const mulib_minimpeg_video_limits_t *limits,
    mulib_minimpeg_video_clip_t *out)
{
  (void)data; (void)size; (void)limits; (void)out;
  return MULIB_MINIMPEG_VIDEO_UNAVAILABLE;
}
#endif
#endif
