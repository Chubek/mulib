/* MuLIB compiler tools: Ordered generated-to-original source-span mappings.
 * ISO C11 single-header module. Define MULIB_IMPL_SOURCE_MAP in ONE translation unit.
 * Bounded operations report failure instead of silently truncating data.
 */
#ifndef MULIB_SOURCE_MAP_H
#define MULIB_SOURCE_MAP_H
#include <stddef.h>
#include <stdint.h>
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct {size_t generated, length, original, source_id;
} mulib_source_map_entry_t;
typedef struct {mulib_source_map_entry_t *entries;
size_t count,capacity;
} mulib_source_map_t;
int mulib_source_map_add(mulib_source_map_t *m,size_t gen,size_t length,size_t source_id,size_t orig);
int mulib_source_map_find(const mulib_source_map_t *m,size_t offset,mulib_source_map_entry_t *out);
void mulib_source_map_free(mulib_source_map_t *m);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_SOURCE_MAP_H */
#if defined(MULIB_IMPL_SOURCE_MAP) && !defined(MULIB_SOURCE_MAP_IMPLEMENTED)
#define MULIB_SOURCE_MAP_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_source_map_add(mulib_source_map_t *m,size_t gen,size_t n,size_t src,size_t orig){
 if(!m||!n||gen>SIZE_MAX-n||orig>SIZE_MAX-n||(m->count&&gen<m->entries[m->count-1].generated+m->entries[m->count-1].length))return 0;
 if(m->count==m->capacity){size_t z=m->capacity?m->capacity*2:16;
if(z<m->capacity||z>SIZE_MAX/sizeof(*m->entries))return 0;
void *p=realloc(m->entries,z*sizeof(*m->entries));
if(!p)return 0;
m->entries=(mulib_source_map_entry_t*)p;
m->capacity=z;
}
 m->entries[m->count++]=(mulib_source_map_entry_t){gen,n,orig,src};
return 1;
}
int mulib_source_map_find(const mulib_source_map_t *m,size_t x,mulib_source_map_entry_t *out){
 if(!m||!out)return 0;
size_t lo=0,hi=m->count;
while(lo<hi){size_t mid=lo+(hi-lo)/2;
if(m->entries[mid].generated<=x)lo=mid+1;
else hi=mid;
}
 if(!lo)return 0;
mulib_source_map_entry_t e=m->entries[lo-1];
if(x-e.generated>=e.length)return 0;
*out=e;
out->original+=x-e.generated;
out->generated=x;
out->length=1;
return 1;
}
void mulib_source_map_free(mulib_source_map_t *m){if(m){free(m->entries);
memset(m,0,sizeof(*m));
}}
#ifdef __cplusplus
}
#endif
#endif
