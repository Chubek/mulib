/** @file mulib-typecontext.h
 * @brief Reusable context owning a document working byte arena.
 * ISO C11, no third-party libraries. Define MULIB_IMPL_TYPECONTEXT once.
 * Public implementation depends on mulib-typecore.h (MULIB_IMPL_TYPECORE).
 * See manual/mulib-typecontext.md for scope and limitations.
 */
#ifndef MULIB_TYPECONTEXT_H
#define MULIB_TYPECONTEXT_H
#include "mulib-typecore.h"
#include <math.h>
#include <ctype.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_typecontext_context {mulib_typecore_buf_t scratch;size_t max_bytes;} mulib_typecontext_context_t;
void mulib_typecontext_init(mulib_typecontext_context_t *ctx,size_t maximum);
int mulib_typecontext_append(mulib_typecontext_context_t *ctx,const void *bytes,size_t size);
void mulib_typecontext_reset(mulib_typecontext_context_t *ctx);
void mulib_typecontext_destroy(mulib_typecontext_context_t *ctx);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_TYPECONTEXT_H */
#if defined(MULIB_IMPL_TYPECONTEXT) && !defined(MULIB_TYPECONTEXT_H_IMPLEMENTED)
#define MULIB_TYPECONTEXT_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
void mulib_typecontext_init(mulib_typecontext_context_t *c,size_t max){if(c){memset(c,0,sizeof(*c));c->max_bytes=max;}}
int mulib_typecontext_append(mulib_typecontext_context_t *c,const void *p,size_t n){return c&&c->scratch.size<=c->max_bytes&&n<=c->max_bytes-c->scratch.size&&mulib_typecore_append(&c->scratch,p,n);}
void mulib_typecontext_reset(mulib_typecontext_context_t *c){if(c)c->scratch.size=0;}
void mulib_typecontext_destroy(mulib_typecontext_context_t *c){if(c){mulib_typecore_free(&c->scratch);memset(c,0,sizeof(*c));}}
#ifdef __cplusplus
}
#endif
#endif
