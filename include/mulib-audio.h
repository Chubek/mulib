/** @file mulib-audio.h
 * @brief Shared owned interleaved PCM, codec limits and binary file buffers.
 */
#ifndef MULIB_AUDIO_H
#define MULIB_AUDIO_H
#include "mulib-xsimd.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef enum mulib_audio_status {
  MULIB_AUDIO_OK, MULIB_AUDIO_INVALID, MULIB_AUDIO_FORMAT,
  MULIB_AUDIO_UNSUPPORTED, MULIB_AUDIO_LIMIT, MULIB_AUDIO_MEMORY,
  MULIB_AUDIO_IO, MULIB_AUDIO_SCRIPT
} mulib_audio_status_t;
/** Owned interleaved normalized float PCM. Zero-initialize before first use.
 * frames counts samples per channel; rate is Hz. */
typedef struct mulib_audio_pcm {
  float *samples;
  size_t frames;
  unsigned rate, channels;
} mulib_audio_pcm_t;
/** Owned bytes. Zero-initialize; release with mulib_audio_bytes_free. */
typedef struct mulib_audio_bytes { unsigned char *data; size_t size, capacity; } mulib_audio_bytes_t;
typedef struct mulib_audio_options {
  mulib_xsimd_backend_t simd; /**< AUTO by default; unavailable explicit ISA fails. */
  size_t max_frames; /**< Zero selects 16777216 frames. */
} mulib_audio_options_t;
/** Allocate silence. Output must be empty; supports 1..8 channels, 1..768000 Hz. */
mulib_audio_status_t mulib_audio_pcm_create (mulib_audio_pcm_t *, size_t frames, unsigned rate, unsigned channels);
/** Free and zero an owned buffer; NULL safe. */
void mulib_audio_pcm_free (mulib_audio_pcm_t *);
void mulib_audio_bytes_free (mulib_audio_bytes_t *);
/** Validate shape and finite PCM samples, plus limits/backend. */
mulib_audio_status_t mulib_audio_validate (const mulib_audio_pcm_t *, const mulib_audio_options_t *);
/** Append bytes, preserving the original buffer on failure. Source must not alias it. */
mulib_audio_status_t mulib_audio_append (mulib_audio_bytes_t *, const void *, size_t);
/** Read at most limit bytes (zero selects 256 MiB); output must be empty. */
mulib_audio_status_t mulib_audio_read_file (const char *, size_t limit, mulib_audio_bytes_t *);
/** Write complete bytes; an IO failure can leave a partial file. */
mulib_audio_status_t mulib_audio_write_file (const char *, const void *, size_t);
/** Static status description. */
const char *mulib_audio_status_string (mulib_audio_status_t);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_AUDIO) && !defined(MULIB_AUDIO_IMPLEMENTED)
#define MULIB_AUDIO_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
mulib_audio_status_t mulib_audio_pcm_create (mulib_audio_pcm_t *p, size_t n, unsigned r, unsigned c)
{
  float *s = NULL;
  if (!p || p->samples || p->frames || !r || r>768000 || !c || c>8) return MULIB_AUDIO_INVALID;
  if (n>SIZE_MAX/c/sizeof(float)) return MULIB_AUDIO_LIMIT;
  if (n && !(s=(float *)calloc(n*c,sizeof(float)))) return MULIB_AUDIO_MEMORY;
  p->samples=s; p->frames=n; p->rate=r; p->channels=c; return MULIB_AUDIO_OK;
}
void mulib_audio_pcm_free (mulib_audio_pcm_t *p)
{ if (p) { free(p->samples); memset(p,0,sizeof(*p)); } }
void mulib_audio_bytes_free (mulib_audio_bytes_t *p)
{ if (p) { free(p->data); memset(p,0,sizeof(*p)); } }
mulib_audio_status_t mulib_audio_validate (const mulib_audio_pcm_t *p, const mulib_audio_options_t *o)
{
  size_t i, max = o && o->max_frames ? o->max_frames : 16777216;
  if (!p || !p->rate || p->rate>768000 || !p->channels || p->channels>8 || (!p->samples && p->frames)) return MULIB_AUDIO_INVALID;
  if (!mulib_xsimd_available(o ? o->simd : MULIB_XSIMD_AUTO)) return MULIB_AUDIO_UNSUPPORTED;
  if (p->frames>max || p->frames>SIZE_MAX/p->channels/sizeof(float)) return MULIB_AUDIO_LIMIT;
  for (i=0; i<p->frames*p->channels; ++i) if (!isfinite(p->samples[i])) return MULIB_AUDIO_INVALID;
  return MULIB_AUDIO_OK;
}
mulib_audio_status_t mulib_audio_append (mulib_audio_bytes_t *b, const void *p, size_t n)
{
  size_t cap; unsigned char *v;
  if (!b || (!p && n) || b->size>b->capacity || (!b->data && b->capacity)) return MULIB_AUDIO_INVALID;
  if (n>SIZE_MAX-b->size) return MULIB_AUDIO_LIMIT;
  if (b->size+n>b->capacity) {
    cap=b->capacity ? b->capacity : 256;
    while (cap<b->size+n) { if (cap>SIZE_MAX/2) { cap=b->size+n; break; } cap*=2; }
    v=(unsigned char *)realloc(b->data,cap); if (!v) return MULIB_AUDIO_MEMORY;
    b->data=v; b->capacity=cap;
  }
  if (n) memcpy(b->data+b->size,p,n);
  b->size+=n; return MULIB_AUDIO_OK;
}
mulib_audio_status_t mulib_audio_read_file (const char *path, size_t limit, mulib_audio_bytes_t *out)
{
  FILE *f; unsigned char block[8192]; size_t n; mulib_audio_bytes_t b={0}; mulib_audio_status_t st=MULIB_AUDIO_OK;
  if (!path || !out || out->data || out->size || out->capacity) return MULIB_AUDIO_INVALID;
  if (!limit) limit=268435456;
  f=fopen(path,"rb"); if (!f) return MULIB_AUDIO_IO;
  while ((n=fread(block,1,sizeof(block),f))!=0) {
    if (n>limit-b.size) { st=MULIB_AUDIO_LIMIT; break; }
    st=mulib_audio_append(&b,block,n); if (st) break;
  }
  if (ferror(f)) st=MULIB_AUDIO_IO;
  if (fclose(f)) st=MULIB_AUDIO_IO;
  if (st) mulib_audio_bytes_free(&b); else *out=b;
  return st;
}
mulib_audio_status_t mulib_audio_write_file (const char *path, const void *p, size_t n)
{
  FILE *f; int ok;
  if (!path || (!p && n)) return MULIB_AUDIO_INVALID;
  f=fopen(path,"wb"); if (!f) return MULIB_AUDIO_IO;
  ok=!n || fwrite(p,1,n,f)==n; if (fclose(f)) ok=0;
  return ok ? MULIB_AUDIO_OK : MULIB_AUDIO_IO;
}
const char *mulib_audio_status_string (mulib_audio_status_t s)
{
  switch (s) {
  case MULIB_AUDIO_OK: return "ok";
  case MULIB_AUDIO_INVALID: return "invalid argument or sample";
  case MULIB_AUDIO_FORMAT: return "malformed audio";
  case MULIB_AUDIO_UNSUPPORTED: return "unsupported format feature or SIMD backend";
  case MULIB_AUDIO_LIMIT: return "audio resource limit";
  case MULIB_AUDIO_MEMORY: return "allocation failed";
  case MULIB_AUDIO_IO: return "file IO failed";
  case MULIB_AUDIO_SCRIPT: return "Lua script failed";
  default: return "unknown audio status";
  }
}
#endif
