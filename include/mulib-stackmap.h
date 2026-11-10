/* MuLIB compiler tools: Owned native-PC safepoint maps of GC pointer registers and spill slots.
 * ISO C11 single-header module. Define MULIB_IMPL_STACKMAP in ONE translation unit.
 * Bounded operations report failure instead of silently truncating data.
 */
#ifndef MULIB_STACKMAP_H
#define MULIB_STACKMAP_H
#include <stddef.h>
#include <stdint.h>
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct {size_t pc;
uint64_t pointer_registers;
uint64_t pointer_spills;
} mulib_stackmap_entry_t;
typedef struct {mulib_stackmap_entry_t *entries;
size_t count,capacity;
} mulib_stackmap_t;
int mulib_stackmap_add(mulib_stackmap_t *m,size_t pc,uint64_t registers,uint64_t spills);
int mulib_stackmap_find(const mulib_stackmap_t *m,size_t pc,mulib_stackmap_entry_t *out);
void mulib_stackmap_free(mulib_stackmap_t *m);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_STACKMAP_H */
#if defined(MULIB_IMPL_STACKMAP) && !defined(MULIB_STACKMAP_IMPLEMENTED)
#define MULIB_STACKMAP_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_stackmap_add(mulib_stackmap_t *m,size_t pc,uint64_t regs,uint64_t slots){if(!m||(m->count&&pc<=m->entries[m->count-1].pc))return 0;
if(m->count==m->capacity){size_t c=m->capacity?m->capacity*2:8;
if(c<m->capacity||c>SIZE_MAX/sizeof(*m->entries))return 0;
void *p=realloc(m->entries,c*sizeof(*m->entries));
if(!p)return 0;
m->entries=(mulib_stackmap_entry_t*)p;
m->capacity=c;
}m->entries[m->count++]=(mulib_stackmap_entry_t){pc,regs,slots};
return 1;
}
int mulib_stackmap_find(const mulib_stackmap_t *m,size_t pc,mulib_stackmap_entry_t *out){if(!m||!out)return 0;
size_t lo=0,hi=m->count;
while(lo<hi){size_t mid=lo+(hi-lo)/2;
if(m->entries[mid].pc<pc)lo=mid+1;
else hi=mid;
}if(lo>=m->count||m->entries[lo].pc!=pc)return 0;
*out=m->entries[lo];
return 1;
}
void mulib_stackmap_free(mulib_stackmap_t *m){if(m){free(m->entries);
memset(m,0,sizeof(*m));
}}
#ifdef __cplusplus
}
#endif
#endif
