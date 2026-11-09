/** @file mulib-browsersnapshot.h
 * @brief Owned immutable byte snapshot with FNV-1a change fingerprint.
 * C11 single-header implementation. Define MULIB_IMPL_BROWSERSNAPSHOT in one translation unit.
 */
#ifndef MULIB_BROWSERSNAPSHOT_H
#define MULIB_BROWSERSNAPSHOT_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_browsersnapshot {unsigned char *data;size_t size;uint64_t hash;} mulib_browsersnapshot_t;
int mulib_browsersnapshot_create(mulib_browsersnapshot_t *s,const void *data,size_t size);
void mulib_browsersnapshot_free(mulib_browsersnapshot_t *s);

#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_BROWSERSNAPSHOT) && !defined(MULIB_IMPL_BROWSERSNAPSHOT_IMPLEMENTED)
#define MULIB_IMPL_BROWSERSNAPSHOT_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <limits.h>
#ifdef __cplusplus
extern "C" {
#endif

int mulib_browsersnapshot_create(mulib_browsersnapshot_t *s,const void *p,size_t n){if(!s||(!p&&n)||n>67108864)return 0;unsigned char *b=(unsigned char*)malloc(n?n:1);if(!b)return 0;if(n)memcpy(b,p,n);uint64_t h=UINT64_C(14695981039346656037);for(size_t i=0;i<n;i++){h^=b[i];h*=UINT64_C(1099511628211);}s->data=b;s->size=n;s->hash=h;return 1;}
void mulib_browsersnapshot_free(mulib_browsersnapshot_t *s){if(s){free(s->data);memset(s,0,sizeof *s);}}

#ifdef __cplusplus
}
#endif
#endif
