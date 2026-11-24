/* MuLIB: wave-store. C11 single-header kernel.
 * Define MULIB_IMPL_WAVE_STORE in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_WAVE_STORE_H
#define MULIB_WAVE_STORE_H
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <float.h>
#include <limits.h>
#include <ctype.h>
#include <errno.h>
#include "mulib-storage-core.h"
#include "mulib-num-core.h"
#include "mulib-graph-core.h"
#ifdef __cplusplus
extern "C" {
#endif
#define MULIB_WAVE_WAVE_CHUNK 1024
typedef struct {double time,value;} mulib_wave_sample_record;
typedef struct mulib_wave_chunk {struct mulib_wave_chunk *next;size_t used;mulib_wave_sample_record samples[MULIB_WAVE_WAVE_CHUNK];} mulib_wave_chunk;
typedef struct {mulib_wave_chunk *first,*last;size_t n;} mulib_wave_store;
int mulib_wave_store_append(mulib_wave_store *s,double time,double value);
int mulib_wave_store_get(const mulib_wave_store *s,size_t index,mulib_wave_sample_record *out);
void mulib_wave_store_free(mulib_wave_store *s);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_WAVE_STORE) && !defined(MULIB_WAVE_STORE_H_IMPLEMENTED)
#define MULIB_WAVE_STORE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_wave_store_append(mulib_wave_store *s,double time,double value){if(!s||!isfinite(time)||!isfinite(value)||(s->n&&time<=s->last->samples[s->last->used-1].time)||s->n==SIZE_MAX)return 0;if(!s->last||s->last->used==MULIB_WAVE_WAVE_CHUNK){mulib_wave_chunk *c=(mulib_wave_chunk*)mulib_storage_alloc(1,sizeof *c);if(!c)return 0;if(s->last)s->last->next=c;else s->first=c;s->last=c;}s->last->samples[s->last->used++]=(mulib_wave_sample_record){time,value};s->n++;return 1;}
int mulib_wave_store_get(const mulib_wave_store *s,size_t index,mulib_wave_sample_record *out){if(!s||index>=s->n||!out)return 0;const mulib_wave_chunk *c=s->first;while(c&&index>=c->used){index-=c->used;c=c->next;}if(!c)return 0;*out=c->samples[index];return 1;}
void mulib_wave_store_free(mulib_wave_store *s){if(s){mulib_wave_chunk *c=s->first;while(c){mulib_wave_chunk *next=c->next;free(c);c=next;}memset(s,0,sizeof *s);}}
#ifdef __cplusplus
}
#endif
#endif
