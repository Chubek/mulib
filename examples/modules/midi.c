#define MULIB_IMPL_MIDI
#define MULIB_IMPL_AUDIO
#define MULIB_IMPL_XSIMD
#include "mulib-midi.h"
#include <assert.h>
int main(void)
{
  mulib_midi_t song={0}; mulib_audio_bytes_t bytes={0}; unsigned i;
  const unsigned keys[4]={60,64,67,72};
  assert(mulib_midi_init(&song,480,2)==MULIB_AUDIO_OK);
  assert(mulib_midi_tempo(&song,0,0,500000)==MULIB_AUDIO_OK);
  for(i=0;i<4;++i) assert(mulib_midi_note(&song,1,i*480,432,0,keys[i],100)==MULIB_AUDIO_OK);
  assert(mulib_midi_encode(&song,&bytes)==MULIB_AUDIO_OK);
  assert(mulib_audio_write_file("song.mid",bytes.data,bytes.size)==MULIB_AUDIO_OK);
  mulib_audio_bytes_free(&bytes); mulib_midi_free(&song); return 0;
}
