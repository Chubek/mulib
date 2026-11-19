/* MuLIB / Amb3r: wave-store. C11 single-header kernel.
 * Define MULIB_IMPL_WAVE_STORE in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_WAVE_STORE_H
#define MULIB_WAVE_STORE_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif
#define MA_WAVE_CHUNK 1024
typedef struct {double time,value;} ma_wave_sample;
typedef struct ma_wave_chunk {struct ma_wave_chunk *next;size_t used;ma_wave_sample samples[MA_WAVE_CHUNK];} ma_wave_chunk;
typedef struct {ma_wave_chunk *first,*last;size_t n;} ma_wave_store;
int mulib_wave_store_append(ma_wave_store *s,double time,double value);
int mulib_wave_store_get(const ma_wave_store *s,size_t index,ma_wave_sample *out);
void mulib_wave_store_free(ma_wave_store *s);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_WAVE_STORE) && !defined(MULIB_WAVE_STORE_H_IMPLEMENTED)
#define MULIB_WAVE_STORE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_wave_store_append(ma_wave_store *s,double time,double value){if(!s||!isfinite(time)||!isfinite(value)||(s->n&&time<=s->last->samples[s->last->used-1].time)||s->n==SIZE_MAX)return 0;if(!s->last||s->last->used==MA_WAVE_CHUNK){ma_wave_chunk *c=(ma_wave_chunk*)ma_alloc(1,sizeof *c);if(!c)return 0;if(s->last)s->last->next=c;else s->first=c;s->last=c;}s->last->samples[s->last->used++]=(ma_wave_sample){time,value};s->n++;return 1;}
int mulib_wave_store_get(const ma_wave_store *s,size_t index,ma_wave_sample *out){if(!s||index>=s->n||!out)return 0;const ma_wave_chunk *c=s->first;while(c&&index>=c->used){index-=c->used;c=c->next;}if(!c)return 0;*out=c->samples[index];return 1;}
void mulib_wave_store_free(ma_wave_store *s){if(s){ma_wave_chunk *c=s->first;while(c){ma_wave_chunk *next=c->next;free(c);c=next;}memset(s,0,sizeof *s);}}
#ifdef __cplusplus
}
#endif
#endif
