/* MuLIB notebook desktop extension. C11, header-only.
 * SPDX-License-Identifier: MIT
 * See AGENTS.md for integration and coverage limits. */
#ifndef MULIB_NB_STREAM_H
#define MULIB_NB_STREAM_H
#include "mulib-wm-core.h"
/* Bounded binary ring, no silent loss: write reports accepted byte count. */
typedef struct {unsigned char *p;size_t cap,head,n;} mulib_nb_stream;
static inline int mulib_nb_stream_init(mulib_nb_stream *s,size_t capacity){if(!s||!capacity)return 0;unsigned char *p=(unsigned char*)malloc(capacity);if(!p)return 0;*s=(mulib_nb_stream){p,capacity,0,0};return 1;}
static inline size_t mulib_nb_stream_write(mulib_nb_stream *s,const void *p,size_t n){if(!s||!s->cap||(!p&&n))return 0;if(n>s->cap-s->n)n=s->cap-s->n;const unsigned char *b=(const unsigned char*)p;size_t pos=s->n>=s->cap-s->head?s->n-(s->cap-s->head):s->head+s->n;for(size_t i=0;i<n;i++){s->p[pos]=b[i];if(++pos==s->cap)pos=0;}s->n+=n;return n;}
static inline size_t mulib_nb_stream_read(mulib_nb_stream *s,void *p,size_t n){if(!s||(!p&&n))return 0;if(n>s->n)n=s->n;unsigned char *b=(unsigned char*)p;for(size_t i=0;i<n;i++){b[i]=s->p[s->head];if(++s->head==s->cap)s->head=0;}s->n-=n;return n;}
static inline void mulib_nb_stream_free(mulib_nb_stream *s){if(s){free(s->p);memset(s,0,sizeof *s);}}
#endif
