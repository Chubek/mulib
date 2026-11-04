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
#include <string.h>

static mulib_mune_status_t property(mulib_mune_state_t *s, const mulib_mune_value_t *args,
    size_t count, mulib_mune_result_t *out, void *user)
{
  (void)args; assert(count==2); mulib_mune_collect(s);
  return mulib_mune_result_push(out,mulib_mune_integer(*(int *)user))?MULIB_MUNE_OK:MULIB_MUNE_MEMORY;
}
static void bindings(mulib_mune_state_t *s)
{
  mulib_mune_value_t object, meta, fn, owner, v, key; void *data; size_t cursor=0, count=0;
  mulib_mune_result_t *root=mulib_mune_result_create(s), *result=NULL;
  mulib_mune_state_t *other=mulib_mune_state_create(NULL);
  assert(root && other);
  assert(mulib_mune_make_table(s,&object)==MULIB_MUNE_OK);
  assert(mulib_mune_result_push(root,object));
  assert(mulib_mune_make_table(s,&meta)==MULIB_MUNE_OK);
  assert(mulib_mune_result_push(root,meta));
  assert(mulib_mune_make_userdata(s,sizeof(int),&owner,&data)==MULIB_MUNE_OK);
  *(int *)data=42;
  assert(mulib_mune_make_function(s,property,data,&fn)==MULIB_MUNE_OK);
  assert(mulib_mune_function_owner(s,fn,owner)==MULIB_MUNE_OK);
  assert(mulib_mune_set_field(s,meta,"__index",fn)==MULIB_MUNE_OK);
  assert(mulib_mune_set_metatable(s,object,meta)==MULIB_MUNE_OK);
  assert(mulib_mune_get_metatable(s,object,&v)==MULIB_MUNE_OK && v.as.object==meta.as.object);
  assert(mulib_mune_set_global(s,"object",object)==MULIB_MUNE_OK);
  assert(mulib_mune_get_field(s,object,"answer",&v)==MULIB_MUNE_OK && v.as.integer==42);
  assert(mulib_mune_set_uservalue(s,owner,1,mulib_mune_integer(17))==MULIB_MUNE_OK);
  assert(mulib_mune_get_uservalue(s,owner,1,&v)==MULIB_MUNE_OK && v.as.integer==17);
  assert(mulib_mune_set_field(s,object,"direct",mulib_mune_number(0.5))==MULIB_MUNE_OK);
  assert(mulib_mune_get_field(s,object,"direct",&v)==MULIB_MUNE_OK && v.as.number==0.5);
  while(mulib_mune_table_next(s,object,&cursor,&key,&v)==1) ++count;
  assert(count==1);
  assert(mulib_mune_set_field(other,object,"bad",v)==MULIB_MUNE_INVALID);
  assert(mulib_mune_run_source(s,"assert(object.answer == 42)",27,&result,NULL)==MULIB_MUNE_OK);
  mulib_mune_result_destroy(result); result=NULL;
  {
    const char source[]="return setmetatable({}, {__index=function() error('property failed') end})";
    assert(mulib_mune_run_source(s,source,sizeof(source)-1,&result,NULL)==MULIB_MUNE_OK);
    v=mulib_mune_integer(99);
    assert(mulib_mune_get_field(s,mulib_mune_result_get(result,0),"x",&v)==MULIB_MUNE_RUNTIME);
    assert(v.as.integer==99);
    mulib_mune_result_destroy(result);
    assert(mulib_mune_get_field(s,object,"answer",&v)==MULIB_MUNE_OK && v.as.integer==42);
  }
  mulib_mune_result_destroy(root); mulib_mune_state_destroy(other);
}
static mulib_audio_status_t failing_effect(float *samples, size_t frames, unsigned channels, unsigned rate, void *user)
{ (void)frames; (void)channels; (void)rate; (void)user; samples[0]=99; return MULIB_AUDIO_INVALID; }
int main(void)
{
  mulib_mune_state_t *s=mulib_mune_state_create(NULL); mulib_audio_pcm_t p={0}, q={0}, rendered={0};
  mulib_audrey_filter_t filter={0}; mulib_mune_error_t error; mulib_midi_t midi={0}; size_t i;
  const char effect[]="local n=0; return function(sample, channel, frame, rate) n=n+1; assert(rate==44100); return sample*0.5 end";
  const char bad[]="return function(sample, channel, frame) if frame==2 then error('bad frame') end return sample*2 end";
  const char score[]="return {tempo=120,ppqn=480,tracks={{channel=2,program=10,notes={{beat=0,duration=1,key=60},{beat=1,duration=0.5,key=64,velocity=80}}}}}";
  assert(s); bindings(s);
  assert(mulib_audio_pcm_create(&p,8,44100,2)==MULIB_AUDIO_OK);
  for(i=0;i<16;++i) p.samples[i]=(float)i/16;
  assert(mulib_audrey_slice(&p,2,3,&q)==MULIB_AUDIO_OK && q.samples[0]==0.25f);
  assert(mulib_audrey_cut(&q,1,1)==MULIB_AUDIO_OK && q.frames==2 && q.samples[2]==0.5f);
  assert(mulib_audrey_insert(&q,1,&q)==MULIB_AUDIO_OK && q.frames==4 && q.samples[6]==0.5f);
  mulib_audio_pcm_free(&q);
  assert(mulib_audrey_slice(&p,9,0,&q)==MULIB_AUDIO_INVALID);
  assert(mulib_audrey_resample(&p,88200,&q)==MULIB_AUDIO_OK && q.frames==16 && q.samples[2]==0.0625f);
  mulib_audio_pcm_free(&q);
  {
    mulib_audrey_track_t tracks[2]={{1,0,0},{0.5f,0,0}};
    mulib_audrey_clip_t clips[2]={{&p,0,8,0,1,0,0},{&p,0,4,4,1,0,1}};
    assert(mulib_audrey_render(clips,2,tracks,2,8,44100,2,NULL,&rendered)==MULIB_AUDIO_OK);
    assert(rendered.samples[10]==p.samples[10]+0.5f*p.samples[2]); mulib_audio_pcm_free(&rendered);
    tracks[1].solo=1;
    assert(mulib_audrey_render(clips,2,tracks,2,8,44100,2,NULL,&rendered)==MULIB_AUDIO_OK);
    assert(rendered.samples[2]==0 && rendered.samples[10]==0.5f*p.samples[2]); mulib_audio_pcm_free(&rendered);
  }
  assert(mulib_audrey_effect(&p,failing_effect,NULL)==MULIB_AUDIO_INVALID && p.samples[0]==0);
  assert(mulib_audrey_filter_compile(s,effect,sizeof(effect)-1,&filter,&error)==MULIB_AUDIO_OK);
  mulib_mune_collect(s);
  assert(mulib_audrey_filter_apply(&p,&filter,&error)==MULIB_AUDIO_OK && p.samples[10]==0.3125f);
  mulib_audrey_filter_free(&filter);
  assert(mulib_audrey_filter_compile(s,bad,sizeof(bad)-1,&filter,&error)==MULIB_AUDIO_OK);
  assert(mulib_audrey_filter_apply(&p,&filter,&error)==MULIB_AUDIO_SCRIPT && p.samples[2]==0.0625f);
  mulib_audrey_filter_free(&filter);
  assert(mulib_audrey_normalize(&p,1,MULIB_XSIMD_AUTO)==MULIB_AUDIO_OK && p.samples[15]==1);
  assert(mulib_audrey_fade(&p,1,0)==MULIB_AUDIO_OK && p.samples[15]==0);
  assert(mulib_audrey_lowpass(&p,1000)==MULIB_AUDIO_OK);
  assert(mulib_audrey_score(s,score,sizeof(score)-1,&midi,&error)==MULIB_AUDIO_OK);
  assert(midi.tracks==2 && midi.division==480 && midi.count==6 && midi.events[2].status==0x91);
  assert(midi.events[3].tick==480 && midi.events[5].tick==720); mulib_midi_free(&midi);
  {
    const char sparse[]="return {tracks={{notes={[2]={beat=0,duration=1,key=60}}}}}";
    assert(mulib_audrey_score(s,sparse,sizeof(sparse)-1,&midi,&error)==MULIB_AUDIO_INVALID);
    assert(!midi.events);
  }
  mulib_audio_pcm_free(&p); mulib_mune_state_destroy(s); return 0;
}
