#define MULIB_IMPL_AUDREY
#define MULIB_IMPL_AUDIO
#define MULIB_IMPL_XSIMD
#define MULIB_IMPL_MINIWAV
#define MULIB_IMPL_MINIFLAC
#define MULIB_IMPL_MINIVORBIS
#define MULIB_IMPL_MINIMPEG
#define MULIB_IMPL_MIDI
#define MULIB_IMPL_MUNE
#define MULIB_IMPL_DYNLOAD
#define MULIB_IMPL_MIR
#define MULIB_IMPL_VMWEAVE
#define MULIB_IMPL_EGRAPH
#define MULIB_IMPL_REWRITE
#define MULIB_IMPL_DYNCALL
#define MULIB_IMPL_JIT
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_PARSER
#define MULIB_IMPL_PEG
#define MULIB_IMPL_AST
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_GLOB
#include "mulib-audrey.h"
#include <assert.h>
#include <math.h>
int main(void)
{
  mulib_mune_state_t *state=mulib_mune_state_create(NULL);
  mulib_audio_pcm_t pcm={0}; mulib_audrey_filter_t filter={0};
  mulib_midi_t song={0}; mulib_audio_bytes_t bytes={0}; size_t i;
  const char effect[]="return function(x, channel, frame, rate) return x*0.5 end";
  const char score[]=
    "local notes={}\n"
    "for i,key in ipairs({60,64,67,72}) do\n"
    "  notes[i]={beat=i-1,duration=0.9,key=key}\n"
    "end\n"
    "return {tempo=120,tracks={{program=0,notes=notes}}}";
  assert(state);
  assert(mulib_audio_pcm_create(&pcm,4410,44100,1)==MULIB_AUDIO_OK);
  for(i=0;i<pcm.frames;++i) pcm.samples[i]=(float)sin(i*0.0626893772);
  assert(mulib_audrey_filter_compile(state,effect,sizeof(effect)-1,&filter,NULL)==MULIB_AUDIO_OK);
  assert(mulib_audrey_filter_apply(&pcm,&filter,NULL)==MULIB_AUDIO_OK);
  assert(mulib_audrey_fade(&pcm,1,0)==MULIB_AUDIO_OK);
  assert(mulib_audrey_save("filtered.wav","wav",&pcm,16,NULL)==MULIB_AUDIO_OK);
  assert(mulib_audrey_score(state,score,sizeof(score)-1,&song,NULL)==MULIB_AUDIO_OK);
  assert(mulib_midi_encode(&song,&bytes)==MULIB_AUDIO_OK);
  assert(mulib_audio_write_file("score.mid",bytes.data,bytes.size)==MULIB_AUDIO_OK);
  mulib_audio_bytes_free(&bytes); mulib_midi_free(&song);
  mulib_audrey_filter_free(&filter); mulib_audio_pcm_free(&pcm);
  mulib_mune_state_destroy(state); return 0;
}
