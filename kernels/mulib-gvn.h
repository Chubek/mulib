/* MuLIB compiler tools: Pure expression value numbering with commutative canonicalization.
 * ISO C11 single-header module. Define MULIB_IMPL_GVN in ONE translation unit.
 * Bounded operations report failure instead of silently truncating data.
 */
#ifndef MULIB_GVN_H
#define MULIB_GVN_H
#include <stddef.h>
#include <stdint.h>
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct {unsigned opcode;
uint64_t a,b;
size_t value;
} mulib_gvn_entry_t;
typedef struct {mulib_gvn_entry_t *entries;
size_t count,capacity;
} mulib_gvn_t;
/* commutative=true normalizes a,b order; returns existing or inserted ID. */
int mulib_gvn_intern(mulib_gvn_t *g,unsigned op,uint64_t a,uint64_t b,int commutative,size_t *value);
void mulib_gvn_free(mulib_gvn_t *g);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_GVN_H */
#if defined(MULIB_IMPL_GVN) && !defined(MULIB_GVN_IMPLEMENTED)
#define MULIB_GVN_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_gvn_intern(mulib_gvn_t *g,unsigned op,uint64_t a,uint64_t b,int commutative,size_t *out){
 if(!g||!out)return 0;
if(commutative&&a>b){uint64_t t=a;
a=b;
b=t;
}
 for(size_t i=0;i<g->count;i++)if(g->entries[i].opcode==op&&g->entries[i].a==a&&g->entries[i].b==b){*out=g->entries[i].value;
return 1;
}
 if(g->count==g->capacity){size_t cap=g->capacity?g->capacity*2:16;
if(cap<g->capacity||cap>SIZE_MAX/sizeof(*g->entries))return 0;
void *p=realloc(g->entries,cap*sizeof(*g->entries));
if(!p)return 0;
g->entries=(mulib_gvn_entry_t*)p;
g->capacity=cap;
}
 size_t value=g->count;
g->entries[g->count++]=(mulib_gvn_entry_t){op,a,b,value};
*out=value;
return 1;
}
void mulib_gvn_free(mulib_gvn_t *g){if(g){free(g->entries);
memset(g,0,sizeof(*g));
}}
#ifdef __cplusplus
}
#endif
#endif
