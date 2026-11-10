/** @file mulib-typesrc.h
 * @brief Owned source text with location-aware line and column lookup.
 * ISO C11, no third-party libraries. Define MULIB_IMPL_TYPESRC once.
 * Public implementation depends on mulib-typecore.h (MULIB_IMPL_TYPECORE).
 * See manual/mulib-typesrc.md for scope and limitations.
 */
#ifndef MULIB_TYPESRC_H
#define MULIB_TYPESRC_H
#include "mulib-typecore.h"
#include <math.h>
#include <ctype.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_typesrc_source { char *data; size_t size; } mulib_typesrc_source_t;
int mulib_typesrc_init(mulib_typesrc_source_t *s,const char *bytes,size_t size);
/* Transactional replacement of an initialized source; input may alias old data. */
int mulib_typesrc_assign(mulib_typesrc_source_t *s,const char *bytes,size_t size);
int mulib_typesrc_locate(const mulib_typesrc_source_t *s,size_t offset,size_t *line,size_t *column);
void mulib_typesrc_destroy(mulib_typesrc_source_t *s);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_TYPESRC_H */
#if defined(MULIB_IMPL_TYPESRC) && !defined(MULIB_TYPESRC_H_IMPLEMENTED)
#define MULIB_TYPESRC_H_IMPLEMENTED
#ifndef MULIB_TYPESRC_MALLOC
#define MULIB_TYPESRC_MALLOC malloc
#endif
#ifndef MULIB_TYPESRC_FREE
#define MULIB_TYPESRC_FREE free
#endif
#ifdef __cplusplus
extern "C" {
#endif
int mulib_typesrc_init(mulib_typesrc_source_t *s,const char *v,size_t n){if(!s||(!v&&n)||n==SIZE_MAX)return 0;char *p=(char*)MULIB_TYPESRC_MALLOC(n+1);if(!p)return 0;if(n)memcpy(p,v,n);p[n]=0;s->data=p;s->size=n;return 1;}
int mulib_typesrc_assign(mulib_typesrc_source_t *s,const char *v,size_t n){if(!s||(!s->data&&s->size))return 0;mulib_typesrc_source_t next={0};if(!mulib_typesrc_init(&next,v,n))return 0;MULIB_TYPESRC_FREE(s->data);*s=next;return 1;}
int mulib_typesrc_locate(const mulib_typesrc_source_t *s,size_t at,size_t *line,size_t *col){if(!s||(!s->data&&s->size)||!line||!col||at>s->size)return 0;size_t l=1,c=1;for(size_t i=0;i<at;i++){if(s->data[i]=='\n'){l++;c=1;}else c++;}*line=l;*col=c;return 1;}
void mulib_typesrc_destroy(mulib_typesrc_source_t *s){if(s){MULIB_TYPESRC_FREE(s->data);memset(s,0,sizeof(*s));}}
#ifdef __cplusplus
}
#endif
#endif
