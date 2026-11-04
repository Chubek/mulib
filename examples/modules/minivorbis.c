#define MULIB_IMPL_MINIVORBIS
#define MULIB_IMPL_AUDIO
#define MULIB_IMPL_XSIMD
#include "mulib-minivorbis.h"
#include <assert.h>
#include <math.h>
int main(void)
{
  mulib_audio_pcm_t pcm={0}, decoded={0}; mulib_audio_bytes_t bytes={0}; size_t i;
  assert(mulib_audio_pcm_create(&pcm,4410,44100,1)==MULIB_AUDIO_OK);
  for(i=0;i<pcm.frames;++i) pcm.samples[i]=(float)(0.25*sin(i*0.0626893772));
  assert(mulib_minivorbis_encode(&pcm,NULL,&bytes)==MULIB_AUDIO_OK);
  assert(mulib_audio_write_file("tone.ogg",bytes.data,bytes.size)==MULIB_AUDIO_OK);
  assert(mulib_minivorbis_decode(bytes.data,bytes.size,NULL,&decoded)==MULIB_AUDIO_OK);
  assert(decoded.frames==pcm.frames);
  mulib_audio_pcm_free(&pcm); mulib_audio_pcm_free(&decoded); mulib_audio_bytes_free(&bytes); return 0;
}
