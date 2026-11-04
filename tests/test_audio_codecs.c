#define MULIB_IMPL_XSIMD
#define MULIB_IMPL_AUDIO
#define MULIB_IMPL_MINIWAV
#define MULIB_IMPL_MINIFLAC
#define MULIB_IMPL_MINIVORBIS
#define MULIB_IMPL_MINIMPEG
#define MULIB_IMPL_MIDI
#include "mulib-miniwav.h"
#include "mulib-miniflac.h"
#include "mulib-minivorbis.h"
#include "mulib-minimpeg.h"
#include "mulib-midi.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

typedef mulib_audio_status_t (*decoder_fn)(const void *,size_t,const mulib_audio_options_t *,mulib_audio_pcm_t *);
static void malformed_test(decoder_fn decode, const mulib_audio_bytes_t *bytes)
{
  unsigned char *copy=(unsigned char *)malloc(bytes->size); size_t i,j;
  mulib_audio_options_t options={MULIB_XSIMD_SCALAR,8192}; uint32_t random=12345;
  assert(copy);
  for(i=0;i<256;++i) {
    mulib_audio_pcm_t pcm={0}; mulib_audio_status_t status; size_t n;
    memcpy(copy,bytes->data,bytes->size);
    random=random*1664525u+1013904223u;
    if(i<128) n=i<bytes->size?i:bytes->size-1;
    else { n=bytes->size; copy[random%n]^=(unsigned char)(1u<<(i%8)); }
    status=decode(copy,n,&options,&pcm);
    if(status==MULIB_AUDIO_OK) {
      assert(pcm.frames<=8192 && pcm.channels>=1 && pcm.channels<=8);
      for(j=0;j<pcm.frames*pcm.channels;++j) assert(isfinite(pcm.samples[j]));
    } else assert(!pcm.samples && !pcm.frames);
    mulib_audio_pcm_free(&pcm);
  }
  free(copy);
}
static void backend_test(decoder_fn decode, const mulib_audio_bytes_t *bytes, const mulib_audio_pcm_t *reference)
{
  int backend; size_t i;
  for(backend=MULIB_XSIMD_SCALAR;backend<=MULIB_XSIMD_NEON;++backend) {
    mulib_audio_pcm_t pcm={0}; mulib_audio_options_t options={(mulib_xsimd_backend_t)backend,0};
    if(!mulib_xsimd_available(options.simd)) {
      assert(decode(bytes->data,bytes->size,&options,&pcm)==MULIB_AUDIO_UNSUPPORTED);
      assert(!pcm.samples); continue;
    }
    assert(decode(bytes->data,bytes->size,&options,&pcm)==MULIB_AUDIO_OK);
    assert(pcm.frames==reference->frames && pcm.channels==reference->channels && pcm.rate==reference->rate);
    for(i=0;i<pcm.frames*pcm.channels;++i) assert(fabsf(pcm.samples[i]-reference->samples[i])<1e-5f);
    mulib_audio_pcm_free(&pcm);
  }
}

static void simd_test(void)
{
  float a[37], b[37], scalar[37], vector[37], dot, ref;
  unsigned i; int backend;
  for(i=0;i<37;++i) { a[i]=(float)i/37; b[i]=1-a[i]; scalar[i]=a[i]; }
  assert(mulib_xsimd_mix(MULIB_XSIMD_SCALAR,scalar,b,37,0.125f));
  assert(mulib_xsimd_dot(MULIB_XSIMD_SCALAR,a,b,37,&ref));
  for(backend=MULIB_XSIMD_SCALAR;backend<=MULIB_XSIMD_NEON;++backend) {
    if(!mulib_xsimd_available((mulib_xsimd_backend_t)backend)) continue;
    memcpy(vector,a,sizeof(a));
    assert(mulib_xsimd_mix((mulib_xsimd_backend_t)backend,vector,b,37,0.125f));
    for(i=0;i<37;++i) assert(fabsf(vector[i]-scalar[i])<1e-6f);
    assert(mulib_xsimd_dot((mulib_xsimd_backend_t)backend,a,b,37,&dot)); assert(fabsf(dot-ref)<1e-5f);
  }
  assert(!mulib_xsimd_scale((mulib_xsimd_backend_t)99,vector,a,37,1));
  assert(mulib_xsimd_scale(MULIB_XSIMD_AUTO,NULL,NULL,0,1));
}
int main(int argc, char **argv)
{
  mulib_audio_pcm_t p={0}, q={0}; mulib_audio_bytes_t b={0}; size_t i; unsigned bits;
  mulib_midi_t m={0}, decoded={0}; mulib_audio_options_t limit={MULIB_XSIMD_SCALAR,1};
  simd_test();
  if(argc==3 && !strcmp(argv[1],"--decode-flac")) {
    assert(mulib_audio_read_file(argv[2],0,&b)==MULIB_AUDIO_OK);
    assert(mulib_miniflac_decode(b.data,b.size,NULL,&p)==MULIB_AUDIO_OK);
    mulib_audio_bytes_free(&b);
    assert(mulib_miniwav_encode(&p,24,0,NULL,&b)==MULIB_AUDIO_OK);
    assert(mulib_audio_write_file("decoded-flac.wav",b.data,b.size)==MULIB_AUDIO_OK);
    mulib_audio_pcm_free(&p); mulib_audio_bytes_free(&b); return 0;
  }
  assert(mulib_audio_pcm_create(&p,5003,44100,2)==MULIB_AUDIO_OK);
  for(i=0;i<p.frames;++i) { p.samples[i*2]=(float)(0.3*sin(i*0.06268937721449021)); p.samples[i*2+1]=(float)(0.2*cos(i*0.09403406582173532)); }
  for(bits=8;bits<=32;bits+=8) {
    assert(mulib_miniwav_encode(&p,bits,0,NULL,&b)==MULIB_AUDIO_OK);
    malformed_test(mulib_miniwav_decode,&b);
    assert(mulib_miniwav_decode(b.data,b.size,&limit,&q)==MULIB_AUDIO_LIMIT);
    assert(mulib_miniwav_decode(b.data,b.size,NULL,&q)==MULIB_AUDIO_OK);
    assert(q.frames==p.frames && q.channels==2 && q.rate==44100);
    for(i=0;i<p.frames*2;++i) assert(fabs(p.samples[i]-q.samples[i])<=ldexp(1.0,1-(int)bits)+1e-7);
    assert(mulib_miniwav_decode(b.data,b.size,NULL,&q)==MULIB_AUDIO_INVALID);
    mulib_audio_pcm_free(&q);
    assert(mulib_miniwav_decode(b.data,b.size-1,NULL,&q)==MULIB_AUDIO_FORMAT);
    if(bits==16 && argc==2) assert(mulib_audio_write_file("source.wav",b.data,b.size)==MULIB_AUDIO_OK);
    mulib_audio_bytes_free(&b);
  }
  for(bits=16;bits<=24;bits+=8) {
    assert(mulib_miniflac_encode(&p,bits,NULL,&b)==MULIB_AUDIO_OK);
    malformed_test(mulib_miniflac_decode,&b);
    assert(mulib_miniflac_decode(b.data,b.size,&limit,&q)==MULIB_AUDIO_LIMIT);
    assert(mulib_miniflac_decode(b.data,b.size,NULL,&q)==MULIB_AUDIO_OK);
    assert(q.frames==p.frames);
    for(i=0;i<p.frames*2;++i) assert(fabs(p.samples[i]-q.samples[i])<=ldexp(1.0,1-(int)bits)+1e-7);
    mulib_audio_pcm_free(&q);
    if(argc==2) assert(mulib_audio_write_file(bits==16?"encoded16.flac":"encoded24.flac",b.data,b.size)==MULIB_AUDIO_OK);
    b.data[b.size-1]^=1;
    assert(mulib_miniflac_decode(b.data,b.size,NULL,&q)==MULIB_AUDIO_FORMAT);
    mulib_audio_bytes_free(&b);
  }
  assert(mulib_minivorbis_encode(&p,NULL,&b)==MULIB_AUDIO_OK);
  malformed_test(mulib_minivorbis_decode,&b);
  assert(mulib_minivorbis_decode(b.data,b.size,NULL,&q)==MULIB_AUDIO_OK);
  assert(q.frames==p.frames && q.channels==2);
  backend_test(mulib_minivorbis_decode,&b,&q);
  { double mse=0; for(i=0;i<p.frames*2;++i) mse+=(p.samples[i]-q.samples[i])*(p.samples[i]-q.samples[i]); assert(mse/(p.frames*2)<0.001); }
  if(argc==2) {
    mulib_audio_bytes_t wav={0};
    assert(mulib_audio_write_file("encoded.ogg",b.data,b.size)==MULIB_AUDIO_OK);
    assert(mulib_miniwav_encode(&q,32,1,NULL,&wav)==MULIB_AUDIO_OK);
    assert(mulib_audio_write_file("own-vorbis.wav",wav.data,wav.size)==MULIB_AUDIO_OK); mulib_audio_bytes_free(&wav);
  }
  mulib_audio_pcm_free(&q); b.data[b.size-1]^=1;
  assert(mulib_minivorbis_decode(b.data,b.size,NULL,&q)==MULIB_AUDIO_FORMAT); mulib_audio_bytes_free(&b);
  assert(mulib_minimpeg_encode(&p,NULL,&b)==MULIB_AUDIO_OK);
  malformed_test(mulib_minimpeg_decode,&b);
  assert(mulib_minimpeg_decode(b.data,b.size,NULL,&q)==MULIB_AUDIO_OK);
  assert(q.frames>=p.frames && q.channels==2);
  backend_test(mulib_minimpeg_decode,&b,&q);
  { double power=0; for(i=0;i<q.frames*2;++i) { assert(isfinite(q.samples[i])); power+=q.samples[i]*q.samples[i]; } assert(power>0.001); }
  if(argc==2) {
    mulib_audio_bytes_t wav={0};
    assert(mulib_audio_write_file("encoded.mp3",b.data,b.size)==MULIB_AUDIO_OK);
    assert(mulib_miniwav_encode(&q,32,1,NULL,&wav)==MULIB_AUDIO_OK);
    assert(mulib_audio_write_file("own-mpeg.wav",wav.data,wav.size)==MULIB_AUDIO_OK); mulib_audio_bytes_free(&wav);
  }
  mulib_audio_pcm_free(&q);
  assert(mulib_minimpeg_decode(b.data,b.size-1,NULL,&q)==MULIB_AUDIO_FORMAT); mulib_audio_bytes_free(&b);
  mulib_audio_pcm_free(&p);
  assert(mulib_audio_pcm_create(&p,0,44100,1)==MULIB_AUDIO_OK);
  assert(mulib_miniflac_encode(&p,16,NULL,&b)==MULIB_AUDIO_OK);
  assert(mulib_miniflac_decode(b.data,b.size,NULL,&q)==MULIB_AUDIO_OK); assert(q.frames==0);
  mulib_audio_pcm_free(&q); mulib_audio_bytes_free(&b);
  assert(mulib_minivorbis_encode(&p,NULL,&b)==MULIB_AUDIO_OK);
  assert(mulib_minivorbis_decode(b.data,b.size,NULL,&q)==MULIB_AUDIO_OK); assert(q.frames==0);
  mulib_audio_pcm_free(&q); mulib_audio_bytes_free(&b); mulib_audio_pcm_free(&p);
  /* Odd-size WAV padding and the shortest final FLAC block. */
  assert(mulib_audio_pcm_create(&p,1,8000,1)==MULIB_AUDIO_OK); p.samples[0]=-1;
  assert(mulib_miniwav_encode(&p,8,0,NULL,&b)==MULIB_AUDIO_OK && b.size==46);
  assert(mulib_miniwav_decode(b.data,b.size,NULL,&q)==MULIB_AUDIO_OK && q.samples[0]==-1);
  mulib_audio_pcm_free(&q); mulib_audio_bytes_free(&b);
  assert(mulib_miniflac_encode(&p,16,NULL,&b)==MULIB_AUDIO_OK);
  assert(mulib_miniflac_decode(b.data,b.size,NULL,&q)==MULIB_AUDIO_OK && q.frames==1 && q.samples[0]==-1);
  mulib_audio_pcm_free(&q); mulib_audio_bytes_free(&b);
  p.samples[0]=NAN;
  assert(mulib_miniwav_encode(&p,16,0,NULL,&b)==MULIB_AUDIO_INVALID && !b.data);
  assert(mulib_miniflac_encode(&p,16,NULL,&b)==MULIB_AUDIO_INVALID && !b.data);
  assert(mulib_minivorbis_encode(&p,NULL,&b)==MULIB_AUDIO_INVALID && !b.data);
  assert(mulib_minimpeg_encode(&p,NULL,&b)==MULIB_AUDIO_INVALID && !b.data);
  mulib_audio_pcm_free(&p);
  assert(mulib_miniwav_decode(NULL,0,NULL,&q)==MULIB_AUDIO_FORMAT);
  assert(mulib_miniflac_decode(NULL,0,NULL,&q)==MULIB_AUDIO_FORMAT);
  assert(mulib_minivorbis_decode(NULL,0,NULL,&q)==MULIB_AUDIO_FORMAT);
  assert(mulib_minimpeg_decode(NULL,0,NULL,&q)==MULIB_AUDIO_FORMAT);
  assert(mulib_midi_init(&m,480,2)==MULIB_AUDIO_OK);
  assert(mulib_midi_tempo(&m,0,0,500000)==MULIB_AUDIO_OK);
  assert(mulib_midi_note(&m,1,480,480,0,64,100)==MULIB_AUDIO_OK);
  assert(mulib_midi_note(&m,1,0,480,0,60,100)==MULIB_AUDIO_OK);
  assert(mulib_midi_note(&m,1,UINT32_MAX,1,0,60,100)==MULIB_AUDIO_INVALID);
  assert(mulib_midi_encode(&m,&b)==MULIB_AUDIO_OK);
  assert(mulib_midi_decode(b.data,b.size,0,&decoded)==MULIB_AUDIO_OK);
  assert(decoded.tracks==2 && decoded.division==480 && decoded.count==7);
  mulib_midi_free(&decoded);
  assert(mulib_midi_decode(b.data,b.size,1,&decoded)==MULIB_AUDIO_LIMIT);
  assert(mulib_midi_decode(b.data,b.size-1,0,&decoded)==MULIB_AUDIO_FORMAT);
  mulib_midi_free(&m); mulib_audio_bytes_free(&b);
  {
    /* Channel running status across note events, followed by EOT. */
    static const unsigned char smf[]={
      'M','T','h','d',0,0,0,6,0,0,0,1,1,224,
      'M','T','r','k',0,0,0,12,0,0x90,60,100,0x83,0x60,60,0,0,0xff,0x2f,0};
    assert(mulib_midi_decode(smf,sizeof(smf),0,&m)==MULIB_AUDIO_OK);
    assert(m.count==3 && m.events[1].tick==480 && m.events[1].status==0x90 && m.events[1].data[1]==0);
    mulib_midi_free(&m);
  }
  puts("audio codecs: ok"); return 0;
}
