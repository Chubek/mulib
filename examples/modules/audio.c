#define MULIB_IMPL_AUDIO
#define MULIB_IMPL_XSIMD
#include "mulib-audio.h"
#include <assert.h>
int main(void)
{
  mulib_audio_pcm_t pcm={0}; mulib_audio_bytes_t bytes={0};
  assert(mulib_audio_pcm_create(&pcm,480,48000,2)==MULIB_AUDIO_OK);
  assert(mulib_audio_validate(&pcm,NULL)==MULIB_AUDIO_OK);
  assert(mulib_audio_append(&bytes,"PCM",3)==MULIB_AUDIO_OK);
  assert(bytes.size==3);
  mulib_audio_pcm_free(&pcm); mulib_audio_bytes_free(&bytes); return 0;
}
