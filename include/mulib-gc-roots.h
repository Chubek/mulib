/* MuLIB compiler tools: Owned set of explicit GC object roots and collector invocation adapter.
 * ISO C11 single-header module. Define MULIB_IMPL_GC_ROOTS in ONE translation unit.
 * Bounded operations report failure instead of silently truncating data.
 */
#ifndef MULIB_GC_ROOTS_H
#define MULIB_GC_ROOTS_H
#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include "mulib-gc.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "mulib-gc.h"
typedef struct {mulib_gc_object_t **items;
size_t count,capacity;
} mulib_gc_roots_t;
int mulib_gc_roots_add(mulib_gc_roots_t *r,mulib_gc_object_t *object);
int mulib_gc_roots_remove(mulib_gc_roots_t *r,mulib_gc_object_t *object);
int mulib_gc_roots_collect(mulib_gc_heap_t *heap,const mulib_gc_roots_t *r,size_t *reclaimed);
void mulib_gc_roots_free(mulib_gc_roots_t *r);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_GC_ROOTS_H */
#if defined(MULIB_IMPL_GC_ROOTS) && !defined(MULIB_GC_ROOTS_IMPLEMENTED)
#define MULIB_GC_ROOTS_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_gc_roots_add(mulib_gc_roots_t *r,mulib_gc_object_t *o){if(!r||!o)return 0;
for(size_t i=0;i<r->count;i++)if(r->items[i]==o)return 1;
if(r->count==r->capacity){size_t n=r->capacity?r->capacity*2:8;
if(n<r->capacity||n>SIZE_MAX/sizeof(*r->items))return 0;
void *p=realloc(r->items,n*sizeof(*r->items));
if(!p)return 0;
r->items=(mulib_gc_object_t**)p;
r->capacity=n;
}r->items[r->count++]=o;
return 1;
}
int mulib_gc_roots_remove(mulib_gc_roots_t *r,mulib_gc_object_t *o){if(!r||!o)return 0;
for(size_t i=0;i<r->count;i++)if(r->items[i]==o){memmove(r->items+i,r->items+i+1,(r->count-i-1)*sizeof(*r->items));
r->count--;
return 1;
}
return 0;
}
int mulib_gc_roots_collect(mulib_gc_heap_t *heap,const mulib_gc_roots_t *r,size_t *reclaimed){return heap&&r&&mulib_gc_collect(heap,r->items,r->count,reclaimed);
}
void mulib_gc_roots_free(mulib_gc_roots_t *r){if(r){free(r->items);
memset(r,0,sizeof(*r));
}}
#ifdef __cplusplus
}
#endif
#endif
