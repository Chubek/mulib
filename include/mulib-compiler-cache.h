/* MuLIB compiler tools: Binary-safe owned in-memory compiler artifact cache with explicit fingerprints.
 * ISO C11 single-header module. Define MULIB_IMPL_COMPILER_CACHE in ONE translation unit.
 * Bounded operations report failure instead of silently truncating data.
 */
#ifndef MULIB_COMPILER_CACHE_H
#define MULIB_COMPILER_CACHE_H
#include <stddef.h>
#include <stdint.h>
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct {uint64_t fingerprint;
unsigned char *data;
size_t length;
} mulib_compiler_cache_item_t;
typedef struct {mulib_compiler_cache_item_t *items;
size_t count,capacity;
} mulib_compiler_cache_t;
int mulib_compiler_cache_put(mulib_compiler_cache_t *c,uint64_t hash,const void *data,size_t len);
int mulib_compiler_cache_get(const mulib_compiler_cache_t *c,uint64_t hash,const void **data,size_t *len);
void mulib_compiler_cache_free(mulib_compiler_cache_t *c);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_COMPILER_CACHE_H */
#if defined(MULIB_IMPL_COMPILER_CACHE) && !defined(MULIB_COMPILER_CACHE_IMPLEMENTED)
#define MULIB_COMPILER_CACHE_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_compiler_cache_put(mulib_compiler_cache_t *c,uint64_t key,const void *p,size_t n){if(!c||(!p&&n))return 0;
unsigned char *copy=(unsigned char*)malloc(n?n:1);
if(!copy)return 0;
if(n)memcpy(copy,p,n);
 for(size_t i=0;i<c->count;i++)if(c->items[i].fingerprint==key){free(c->items[i].data);
c->items[i].data=copy;
c->items[i].length=n;
return 1;
}
 if(c->count==c->capacity){size_t cap=c->capacity?c->capacity*2:8;
if(cap<c->capacity||cap>SIZE_MAX/sizeof(*c->items)){free(copy);
return 0;
}void *x=realloc(c->items,cap*sizeof(*c->items));
if(!x){free(copy);
return 0;
}c->items=(mulib_compiler_cache_item_t*)x;
c->capacity=cap;
}
 c->items[c->count++]=(mulib_compiler_cache_item_t){key,copy,n};
return 1;
}
int mulib_compiler_cache_get(const mulib_compiler_cache_t *c,uint64_t key,const void **data,size_t *len){if(!c||!data||!len)return 0;
for(size_t i=0;i<c->count;i++)if(c->items[i].fingerprint==key){*data=c->items[i].data;
*len=c->items[i].length;
return 1;
}
return 0;
}
void mulib_compiler_cache_free(mulib_compiler_cache_t *c){if(!c)return;
for(size_t i=0;i<c->count;i++)free(c->items[i].data);
free(c->items);
memset(c,0,sizeof(*c));
}
#ifdef __cplusplus
}
#endif
#endif
