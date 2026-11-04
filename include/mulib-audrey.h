/** @file mulib-audrey.h
 * @brief Offline PCM editing, track mixing, C/Lua effects and Lua-to-MIDI scores.
 */
#ifndef MULIB_AUDREY_H
#define MULIB_AUDREY_H
#include "mulib-miniwav.h"
#include "mulib-miniflac.h"
#include "mulib-minivorbis.h"
#include "mulib-minimpeg.h"
#include "mulib-midi.h"
#include "mulib-mune.h"
#ifdef __cplusplus
extern "C" {
#endif
/** Borrowed clip placement on a timeline. Source rate/channels must match output.
 * offset and length select source frames, start selects destination frames. */
typedef struct mulib_audrey_clip {
  const mulib_audio_pcm_t *source;
  size_t offset, length, start;
  float gain, pan; /**< pan in [-1,1], stereo balance; gain defaults must be set. */
  unsigned track;
} mulib_audrey_clip_t;
typedef struct mulib_audrey_track { float gain; int mute, solo; } mulib_audrey_track_t;
/** A filter owns a rooted Lua function and borrows its state. Free before state. */
typedef struct mulib_audrey_filter { mulib_mune_state_t *state; mulib_mune_result_t *root; } mulib_audrey_filter_t;
/** Native block effect: mutate interleaved samples, return OK on success. */
typedef mulib_audio_status_t (*mulib_audrey_effect_fn) (float *, size_t frames,
    unsigned channels, unsigned rate, void *user);
/** Copy a half-open source range to an empty output; exact frame units. */
mulib_audio_status_t mulib_audrey_slice (const mulib_audio_pcm_t *, size_t start, size_t length, mulib_audio_pcm_t *);
/** Remove a half-open range, closing the gap. Transactional on failure. */
mulib_audio_status_t mulib_audrey_cut (mulib_audio_pcm_t *, size_t start, size_t length);
/** Insert a same-format buffer at a frame position; self-insertion is supported. */
mulib_audio_status_t mulib_audrey_insert (mulib_audio_pcm_t *, size_t start, const mulib_audio_pcm_t *);
/** Linear interpolation resampling; ceil(frames*new_rate/old_rate) output frames.
 * Not a band-limited mastering resampler. Output must be empty. */
mulib_audio_status_t mulib_audrey_resample (const mulib_audio_pcm_t *, unsigned rate, mulib_audio_pcm_t *);
/** Offline track render, summing overlapping clips; no implicit limiting.
 * Output is empty on entry. Pan applies only to stereo. Solo overrides mute
 * only for other tracks: a muted solo track remains silent. */
mulib_audio_status_t mulib_audrey_render (const mulib_audrey_clip_t *, size_t clips,
    const mulib_audrey_track_t *, size_t tracks, size_t frames, unsigned rate,
    unsigned channels, const mulib_audio_options_t *, mulib_audio_pcm_t *);
/** Apply a native effect to a copy, committing only finite successful output. */
mulib_audio_status_t mulib_audrey_effect (mulib_audio_pcm_t *, mulib_audrey_effect_fn, void *);
/** Peak-normalize (silence unchanged), apply linear fade or one-pole lowpass.
 * Fade endpoints apply to first/last frame; a single frame uses the first. */
mulib_audio_status_t mulib_audrey_normalize (mulib_audio_pcm_t *, float peak, mulib_xsimd_backend_t);
mulib_audio_status_t mulib_audrey_fade (mulib_audio_pcm_t *, float first, float last);
mulib_audio_status_t mulib_audrey_lowpass (mulib_audio_pcm_t *, double cutoff_hz);
/** Compile source returning function(sample,channel,frame,rate)->finite number.
 * channel is one-based; frame is zero-based. Closures retain effect state.
 * Output filter must be zero initialized. Lua diagnostics use Mune ownership. */
mulib_audio_status_t mulib_audrey_filter_compile (mulib_mune_state_t *, const char *, size_t,
    mulib_audrey_filter_t *, mulib_mune_error_t *);
/** Release the function root; NULL safe. */
void mulib_audrey_filter_free (mulib_audrey_filter_t *);
/** Apply a Lua filter transactionally to PCM. Lua side effects persist on error. */
mulib_audio_status_t mulib_audrey_filter_apply (mulib_audio_pcm_t *, const mulib_audrey_filter_t *, mulib_mune_error_t *);
/** Execute a Lua score returning {tempo=120,ppqn=480,tracks={...}}.
 * Each track: {channel=1,program=0,notes={{beat=0,duration=1,key=60,velocity=100}}}.
 * channel is 1..16, program/key 0..127; beats are quarter notes. Tempo/PPQN,
 * channel/program/velocity have the defaults above. Notes must be dense arrays.
 * Produces tempo track plus one track per score track, bounded to 100000 notes.
 * Output must be zero initialized. Score validation failures return INVALID. */
mulib_audio_status_t mulib_audrey_score (mulib_mune_state_t *, const char *, size_t,
    mulib_midi_t *, mulib_mune_error_t *);
/** Load WAV/FLAC/Ogg Vorbis/MP3 by magic; write by explicit format
 * ("wav"/"flac"/"vorbis"/"mp3"). bits is ignored for lossy formats.
 * Codec profile restrictions apply; see the corresponding headers. */
mulib_audio_status_t mulib_audrey_load (const char *, const mulib_audio_options_t *, mulib_audio_pcm_t *);
mulib_audio_status_t mulib_audrey_save (const char *, const char *format,
    const mulib_audio_pcm_t *, unsigned bits, const mulib_audio_options_t *);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_AUDREY) && !defined(MULIB_AUDREY_IMPLEMENTED)
#define MULIB_AUDREY_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
static mulib_audio_status_t mulib_audrey_valid_ (const mulib_audio_pcm_t *p)
{ mulib_audio_options_t o={MULIB_XSIMD_SCALAR,SIZE_MAX}; return mulib_audio_validate(p,&o); }
mulib_audio_status_t mulib_audrey_slice (const mulib_audio_pcm_t *p, size_t at, size_t n, mulib_audio_pcm_t *out)
{
  mulib_audio_status_t st=mulib_audrey_valid_(p); if(st) return st;
  if(at>p->frames || n>p->frames-at) return MULIB_AUDIO_INVALID;
  st=mulib_audio_pcm_create(out,n,p->rate,p->channels); if(st) return st;
  if(n) memcpy(out->samples,p->samples+at*p->channels,n*p->channels*sizeof(float));
  return MULIB_AUDIO_OK;
}
mulib_audio_status_t mulib_audrey_cut (mulib_audio_pcm_t *p, size_t at, size_t n)
{
  mulib_audio_status_t st=mulib_audrey_valid_(p); if(st) return st;
  if(at>p->frames || n>p->frames-at) return MULIB_AUDIO_INVALID;
  if(n) memmove(p->samples+at*p->channels,p->samples+(at+n)*p->channels,(p->frames-at-n)*p->channels*sizeof(float));
  p->frames-=n; return MULIB_AUDIO_OK;
}
mulib_audio_status_t mulib_audrey_insert (mulib_audio_pcm_t *p, size_t at, const mulib_audio_pcm_t *s)
{
  mulib_audio_pcm_t r={0}; mulib_audio_status_t st=mulib_audrey_valid_(p); if(st) return st;
  st=mulib_audrey_valid_(s); if(st) return st;
  if(at>p->frames || p->rate!=s->rate || p->channels!=s->channels) return MULIB_AUDIO_INVALID;
  if(s->frames>SIZE_MAX-p->frames) return MULIB_AUDIO_LIMIT;
  st=mulib_audio_pcm_create(&r,p->frames+s->frames,p->rate,p->channels); if(st) return st;
  if(at) memcpy(r.samples,p->samples,at*p->channels*sizeof(float));
  if(s->frames) memcpy(r.samples+at*p->channels,s->samples,s->frames*p->channels*sizeof(float));
  if(p->frames>at) memcpy(r.samples+(at+s->frames)*p->channels,p->samples+at*p->channels,(p->frames-at)*p->channels*sizeof(float));
  mulib_audio_pcm_free(p); *p=r; return MULIB_AUDIO_OK;
}
mulib_audio_status_t mulib_audrey_resample (const mulib_audio_pcm_t *p, unsigned rate, mulib_audio_pcm_t *out)
{
  size_t n, i; mulib_audio_status_t st=mulib_audrey_valid_(p); if(st) return st;
  if(!rate || rate>768000) return MULIB_AUDIO_INVALID;
  if(p->frames>(UINT64_MAX-p->rate+1)/rate) return MULIB_AUDIO_LIMIT;
  { uint64_t count=((uint64_t)p->frames*rate+p->rate-1)/p->rate; if(count>SIZE_MAX) return MULIB_AUDIO_LIMIT; n=(size_t)count; }
  st=mulib_audio_pcm_create(out,n,rate,p->channels); if(st) return st;
  for(i=0;i<n;++i) {
    uint64_t pos=(uint64_t)i*p->rate; size_t a=(size_t)(pos/rate), b=a+1; unsigned c;
    double f=(double)(pos%rate)/rate;
    if(b>=p->frames) b=p->frames-1;
    for(c=0;c<p->channels;++c) out->samples[i*p->channels+c]=(float)((1-f)*p->samples[a*p->channels+c]+f*p->samples[b*p->channels+c]);
  }
  return MULIB_AUDIO_OK;
}
mulib_audio_status_t mulib_audrey_render (const mulib_audrey_clip_t *clips, size_t count,
    const mulib_audrey_track_t *tracks, size_t nt, size_t frames, unsigned rate,
    unsigned channels, const mulib_audio_options_t *opt, mulib_audio_pcm_t *out)
{
  mulib_audio_pcm_t p={0}; size_t i; int solo=0; mulib_audio_status_t st;
  mulib_xsimd_backend_t simd=opt?opt->simd:MULIB_XSIMD_AUTO;
  if(!out || out->samples || out->frames || (!clips && count) || (!tracks && nt)) return MULIB_AUDIO_INVALID;
  if(!mulib_xsimd_available(simd)) return MULIB_AUDIO_UNSUPPORTED;
  if(frames>(opt&&opt->max_frames?opt->max_frames:16777216)) return MULIB_AUDIO_LIMIT;
  for(i=0;i<nt;++i) { if(!isfinite(tracks[i].gain)) return MULIB_AUDIO_INVALID; solo|=!!tracks[i].solo; }
  for(i=0;i<count;++i) {
    const mulib_audrey_clip_t *c=clips+i;
    st=mulib_audrey_valid_(c->source); if(st) return st;
    if(c->track>=nt || !isfinite(c->gain) || !isfinite(c->pan) || c->pan<-1 || c->pan>1
        || (channels!=2 && c->pan!=0) || c->source->rate!=rate || c->source->channels!=channels
        || c->offset>c->source->frames || c->length>c->source->frames-c->offset || c->start>frames || c->length>frames-c->start) return MULIB_AUDIO_INVALID;
  }
  st=mulib_audio_pcm_create(&p,frames,rate,channels); if(st) return st;
  for(i=0;i<count;++i) {
    const mulib_audrey_clip_t *c=clips+i; const mulib_audrey_track_t *t=tracks+c->track; size_t j;
    float gain=c->gain*t->gain;
    if(t->mute || (solo && !t->solo) || !c->length) continue;
    if(c->pan==0) (void)mulib_xsimd_mix(simd,p.samples+c->start*channels,c->source->samples+c->offset*channels,c->length*channels,gain);
    else for(j=0;j<c->length;++j) {
      p.samples[(c->start+j)*2]+=c->source->samples[(c->offset+j)*2]*gain*(c->pan>0?1-c->pan:1);
      p.samples[(c->start+j)*2+1]+=c->source->samples[(c->offset+j)*2+1]*gain*(c->pan<0?1+c->pan:1);
    }
  }
  st=mulib_audrey_valid_(&p); if(st) mulib_audio_pcm_free(&p); else *out=p;
  return st;
}
mulib_audio_status_t mulib_audrey_effect (mulib_audio_pcm_t *p, mulib_audrey_effect_fn fn, void *user)
{
  mulib_audio_pcm_t r={0}; mulib_audio_status_t st;
  if(!fn) return MULIB_AUDIO_INVALID;
  st=mulib_audrey_slice(p,0,p?p->frames:0,&r); if(st) return st;
  st=fn(r.samples,r.frames,r.channels,r.rate,user); if(!st) st=mulib_audrey_valid_(&r);
  if(st) mulib_audio_pcm_free(&r); else { mulib_audio_pcm_free(p); *p=r; } return st;
}
mulib_audio_status_t mulib_audrey_normalize (mulib_audio_pcm_t *p, float peak, mulib_xsimd_backend_t simd)
{
  size_t i; float max=0; mulib_audio_status_t st=mulib_audrey_valid_(p); if(st) return st;
  if(!isfinite(peak) || peak<0 || peak>1) return MULIB_AUDIO_INVALID;
  if(!mulib_xsimd_available(simd)) return MULIB_AUDIO_UNSUPPORTED;
  for(i=0;i<p->frames*p->channels;++i) if(fabsf(p->samples[i])>max) max=fabsf(p->samples[i]);
  if(max) { double gain=(double)peak/max; if(gain<=FLT_MAX) (void)mulib_xsimd_scale(simd,p->samples,p->samples,p->frames*p->channels,(float)gain);
    else for(i=0;i<p->frames*p->channels;++i) p->samples[i]=(float)(p->samples[i]*gain); }
  return MULIB_AUDIO_OK;
}
mulib_audio_status_t mulib_audrey_fade (mulib_audio_pcm_t *p, float first, float last)
{
  size_t i; unsigned c; mulib_audio_status_t st=mulib_audrey_valid_(p); if(st) return st;
  if(!isfinite(first) || !isfinite(last) || first<0 || last<0 || first>1 || last>1) return MULIB_AUDIO_INVALID;
  for(i=0;i<p->frames;++i) { double gain=p->frames>1 ? first+((double)last-first)*i/(p->frames-1) : first;
    for(c=0;c<p->channels;++c) p->samples[i*p->channels+c]=(float)(p->samples[i*p->channels+c]*gain); }
  return MULIB_AUDIO_OK;
}
mulib_audio_status_t mulib_audrey_lowpass (mulib_audio_pcm_t *p, double hz)
{
  double z[8]={0}, a; size_t i; unsigned c; mulib_audio_status_t st=mulib_audrey_valid_(p); if(st) return st;
  if(!isfinite(hz) || hz<=0 || hz>=p->rate/2.0) return MULIB_AUDIO_INVALID;
  a=-expm1(-6.2831853071795864769*hz/p->rate);
  for(i=0;i<p->frames;++i) for(c=0;c<p->channels;++c) { z[c]+=a*(p->samples[i*p->channels+c]-z[c]); p->samples[i*p->channels+c]=(float)z[c]; }
  return MULIB_AUDIO_OK;
}
mulib_audio_status_t mulib_audrey_filter_compile (mulib_mune_state_t *s, const char *source, size_t n,
    mulib_audrey_filter_t *out, mulib_mune_error_t *error)
{
  mulib_mune_result_t *r=NULL;
  if(!s || !out || out->root || out->state || (!source && n)) return MULIB_AUDIO_INVALID;
  if(mulib_mune_run_source(s,source,n,&r,error)) return MULIB_AUDIO_SCRIPT;
  if(mulib_mune_result_get(r,0).type!=MULIB_MUNE_FUNCTION) { mulib_mune_result_destroy(r); return MULIB_AUDIO_INVALID; }
  out->state=s; out->root=r; return MULIB_AUDIO_OK;
}
void mulib_audrey_filter_free (mulib_audrey_filter_t *f)
{ if(f) { mulib_mune_result_destroy(f->root); memset(f,0,sizeof(*f)); } }
mulib_audio_status_t mulib_audrey_filter_apply (mulib_audio_pcm_t *p, const mulib_audrey_filter_t *f, mulib_mune_error_t *error)
{
  mulib_audio_pcm_t r={0}; size_t i; mulib_audio_status_t st;
  if(!f || !f->state || !f->root) return MULIB_AUDIO_INVALID;
  st=mulib_audrey_slice(p,0,p?p->frames:0,&r); if(st) return st;
  if(p->frames>INT64_MAX) { mulib_audio_pcm_free(&r); return MULIB_AUDIO_LIMIT; }
  for(i=0;i<p->frames*p->channels;++i) {
    mulib_mune_value_t args[4], v; mulib_mune_result_t *result=NULL; double value;
    args[0]=mulib_mune_number(p->samples[i]); args[1]=mulib_mune_integer((int64_t)(i%p->channels+1));
    args[2]=mulib_mune_integer((int64_t)(i/p->channels)); args[3]=mulib_mune_integer(p->rate);
    if(mulib_mune_call(f->state,mulib_mune_result_get(f->root,0),args,4,&result,error)) { mulib_mune_result_destroy(result); st=MULIB_AUDIO_SCRIPT; break; }
    v=mulib_mune_result_get(result,0);
    value=v.type==MULIB_MUNE_INTEGER ? (double)v.as.integer : v.type==MULIB_MUNE_NUMBER ? v.as.number : NAN;
    mulib_mune_result_destroy(result);
    if(!isfinite(value) || value>FLT_MAX || value<-FLT_MAX) { st=MULIB_AUDIO_SCRIPT; break; }
    r.samples[i]=(float)value;
  }
  if(st) mulib_audio_pcm_free(&r); else { mulib_audio_pcm_free(p); *p=r; } return st;
}
static int mulib_audrey_field_ (mulib_mune_state_t *s, mulib_mune_value_t t, const char *name, mulib_mune_value_t *out)
{
  mulib_mune_value_t k;
  return !mulib_mune_make_string(s,name,strlen(name),&k) && !mulib_mune_raw_get(s,t,k,out);
}
static int mulib_audrey_number_ (mulib_mune_state_t *s, mulib_mune_value_t t, const char *name, double def, double *out)
{
  mulib_mune_value_t v;
  if(!mulib_audrey_field_(s,t,name,&v)) return 0;
  *out=v.type==MULIB_MUNE_NIL ? def : v.type==MULIB_MUNE_INTEGER ? (double)v.as.integer : v.type==MULIB_MUNE_NUMBER ? v.as.number : NAN;
  return isfinite(*out);
}
static int mulib_audrey_dense_ (mulib_mune_state_t *s, mulib_mune_value_t t, size_t count)
{
  size_t cursor=0, n=0; mulib_mune_value_t k,v; int status;
  while((status=mulib_mune_table_next(s,t,&cursor,&k,&v))==1) {
    if(k.type!=MULIB_MUNE_INTEGER || k.as.integer<1 || (uint64_t)k.as.integer>count) return 0;
    ++n;
  }
  return status==0 && n==count;
}
mulib_audio_status_t mulib_audrey_score (mulib_mune_state_t *s, const char *source, size_t size,
    mulib_midi_t *out, mulib_mune_error_t *error)
{
  mulib_mune_result_t *root=NULL; mulib_mune_value_t score, tracks, track; mulib_midi_t m={0};
  size_t nt=0, i, notes_count=0; double bpm, ppqn; mulib_audio_status_t st=MULIB_AUDIO_INVALID;
  if(!s || !out || out->events || out->count || out->capacity || out->tracks || out->division || (!source && size)) return MULIB_AUDIO_INVALID;
  if(mulib_mune_run_source(s,source,size,&root,error)) return MULIB_AUDIO_SCRIPT;
  score=mulib_mune_result_get(root,0);
  if(!mulib_audrey_number_(s,score,"tempo",120,&bpm) || bpm<4 || bpm>1000
      || !mulib_audrey_number_(s,score,"ppqn",480,&ppqn) || ppqn<1 || ppqn>32767 || floor(ppqn)!=ppqn
      || !mulib_audrey_field_(s,score,"tracks",&tracks) || tracks.type!=MULIB_MUNE_TABLE) goto done;
  for(;;) {
    if(mulib_mune_raw_get(s,tracks,mulib_mune_integer((int64_t)nt+1),&track)) goto done;
    if(track.type==MULIB_MUNE_NIL) break;
    if(track.type!=MULIB_MUNE_TABLE || nt==65534) goto done;
    ++nt;
  }
  if(!mulib_audrey_dense_(s,tracks,nt)) goto done;
  st=mulib_midi_init(&m,(unsigned)ppqn,(unsigned)nt+1); if(st) goto done;
  st=mulib_midi_tempo(&m,0,0,(uint32_t)round(60000000/bpm)); if(st) goto done;
  for(i=0;i<nt;++i) {
    double channel, program; unsigned char prog; size_t j; mulib_mune_value_t notes, note;
    st=MULIB_AUDIO_INVALID;
    if(mulib_mune_raw_get(s,tracks,mulib_mune_integer((int64_t)i+1),&track)
        || !mulib_audrey_number_(s,track,"channel",1,&channel) || channel<1 || channel>16 || floor(channel)!=channel
        || !mulib_audrey_number_(s,track,"program",0,&program) || program<0 || program>127 || floor(program)!=program
        || !mulib_audrey_field_(s,track,"notes",&notes) || notes.type!=MULIB_MUNE_TABLE) goto done;
    prog=(unsigned char)program; st=mulib_midi_event(&m,(unsigned)i+1,0,0xc0|((unsigned)channel-1),0,&prog,1); if(st) goto done;
    for(j=0;;++j) {
      double beat, duration, key, velocity, tick, end;
      st=MULIB_AUDIO_INVALID;
      if(mulib_mune_raw_get(s,notes,mulib_mune_integer((int64_t)j+1),&note)) goto done;
      if(note.type==MULIB_MUNE_NIL) { if(!mulib_audrey_dense_(s,notes,j)) goto done; st=MULIB_AUDIO_OK; break; }
      if(notes_count++==100000) { st=MULIB_AUDIO_LIMIT; goto done; }
      if(!mulib_audrey_number_(s,note,"beat",NAN,&beat) || beat<0
          || !mulib_audrey_number_(s,note,"duration",NAN,&duration) || duration<=0
          || !mulib_audrey_number_(s,note,"key",NAN,&key) || key<0 || key>127 || floor(key)!=key
          || !mulib_audrey_number_(s,note,"velocity",100,&velocity) || velocity<1 || velocity>127 || floor(velocity)!=velocity) goto done;
      tick=round(beat*ppqn); end=round((beat+duration)*ppqn);
      if(!isfinite(end) || end>UINT32_MAX || end<=tick) goto done;
      st=mulib_midi_note(&m,(unsigned)i+1,(uint32_t)tick,(uint32_t)(end-tick),(unsigned)channel-1,(unsigned)key,(unsigned)velocity);
      if(st) goto done;
      if(j%256==0) (void)mulib_mune_collect(s);
    }
  }
  *out=m; memset(&m,0,sizeof(m)); st=MULIB_AUDIO_OK;
done:
  mulib_midi_free(&m); mulib_mune_result_destroy(root); return st;
}
mulib_audio_status_t mulib_audrey_load (const char *path, const mulib_audio_options_t *opt, mulib_audio_pcm_t *out)
{
  mulib_audio_bytes_t b={0}; mulib_audio_status_t st=mulib_audio_read_file(path,0,&b); if(st) return st;
  if(b.size>=4 && !memcmp(b.data,"fLaC",4)) st=mulib_miniflac_decode(b.data,b.size,opt,out);
  else if(b.size>=4 && !memcmp(b.data,"OggS",4)) st=mulib_minivorbis_decode(b.data,b.size,opt,out);
  else if((b.size>=3 && !memcmp(b.data,"ID3",3)) || (b.size>=2 && b.data[0]==255 && (b.data[1]&224)==224)) st=mulib_minimpeg_decode(b.data,b.size,opt,out);
  else st=mulib_miniwav_decode(b.data,b.size,opt,out);
  mulib_audio_bytes_free(&b); return st;
}
mulib_audio_status_t mulib_audrey_save (const char *path, const char *format, const mulib_audio_pcm_t *p, unsigned bits, const mulib_audio_options_t *opt)
{
  mulib_audio_bytes_t b={0}; mulib_audio_status_t st;
  if(!format || !path) return MULIB_AUDIO_INVALID;
  if(!strcmp(format,"wav")) st=mulib_miniwav_encode(p,bits,0,opt,&b);
  else if(!strcmp(format,"flac")) st=mulib_miniflac_encode(p,bits,opt,&b);
  else if(!strcmp(format,"vorbis")) st=mulib_minivorbis_encode(p,opt,&b);
  else if(!strcmp(format,"mp3")) st=mulib_minimpeg_encode(p,opt,&b);
  else return MULIB_AUDIO_UNSUPPORTED;
  if(!st) st=mulib_audio_write_file(path,b.data,b.size);
  mulib_audio_bytes_free(&b); return st;
}
#endif
