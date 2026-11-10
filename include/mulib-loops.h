/* MuLIB compiler tools: Dominance-based natural-loop membership and backedge identification.
 * ISO C11 single-header module. Define MULIB_IMPL_LOOPS in ONE translation unit.
 * Bounded operations report failure instead of silently truncating data.
 */
#ifndef MULIB_LOOPS_H
#define MULIB_LOOPS_H
#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include "mulib-dominance.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "mulib-dominance.h"
typedef struct {size_t header,tail;
uint64_t members;
} mulib_loop_t;
typedef struct {mulib_loop_t loops[64];
size_t count;
} mulib_loops_t;
int mulib_loops_find(const mulib_cfg_t *g,const mulib_dominance_t *dom,mulib_loops_t *out);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_LOOPS_H */
#if defined(MULIB_IMPL_LOOPS) && !defined(MULIB_LOOPS_IMPLEMENTED)
#define MULIB_LOOPS_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_loops_find(const mulib_cfg_t *g,const mulib_dominance_t *dom,mulib_loops_t *out){
 if(!mulib_cfg_validate(g)||!dom||!out||g->blocks>64||dom->blocks!=g->blocks)return 0;
mulib_loops_t result={0};
 for(size_t e=0;e<g->count;e++){size_t tail=g->edges[e].from,header=g->edges[e].to;
if(tail>=g->blocks||header>=g->blocks)return 0;
if(!mulib_dominance_test(dom,header,tail))continue;
 if(result.count==64)return 0;
uint64_t mask=(UINT64_C(1)<<header)|(UINT64_C(1)<<tail);
int changed;
do{changed=0;
for(size_t i=0;i<g->count;i++){size_t a=g->edges[i].from,b=g->edges[i].to;
 if(b!=header&&(dom->reachable&(UINT64_C(1)<<a))&&(mask&(UINT64_C(1)<<b))&&!(mask&(UINT64_C(1)<<a))){mask|=UINT64_C(1)<<a;
changed=1;
}}}while(changed);
 result.loops[result.count++]=(mulib_loop_t){header,tail,mask};
}
 *out=result;
return 1;
}
#ifdef __cplusplus
}
#endif
#endif
