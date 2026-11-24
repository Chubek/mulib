/* MuLIB compiler tools: Backward register liveness with 64-value block bitsets and fixed-point iteration.
 * ISO C11 single-header module. Define MULIB_IMPL_LIVENESS in ONE translation unit.
 * Bounded operations report failure instead of silently truncating data.
 */
#ifndef MULIB_LIVENESS_H
#define MULIB_LIVENESS_H
#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include "mulib-cfg.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "mulib-cfg.h"
typedef struct {uint64_t use[64],def[64],in[64],out[64];
size_t count;
} mulib_liveness_t;
/* use/def indexed by block; no side effects on failure. */
int mulib_liveness_compute(const mulib_cfg_t *g,const uint64_t *use,const uint64_t *def,mulib_liveness_t *out);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_LIVENESS_H */
#if defined(MULIB_IMPL_LIVENESS) && !defined(MULIB_LIVENESS_IMPLEMENTED)
#define MULIB_LIVENESS_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_liveness_compute(const mulib_cfg_t *g,const uint64_t *use,const uint64_t *def,mulib_liveness_t *out){
 if(!mulib_cfg_validate(g)||!out||(!use&&g->blocks)||(!def&&g->blocks)||g->blocks>64)return 0;
 mulib_liveness_t v;
memset(&v,0,sizeof(v));
v.count=g->blocks;
for(size_t i=0;i<g->blocks;i++){v.use[i]=use[i];
v.def[i]=def[i];
}
 int changed;
size_t iteration=0;
do{changed=0;
for(size_t b=g->blocks;b>0;b--){size_t id=b-1;
uint64_t newout=0;
 for(size_t e=0;e<g->count;e++)if(g->edges[e].from==id){if(g->edges[e].to>=g->blocks)return 0;
newout|=v.in[g->edges[e].to];
}
 uint64_t newin=v.use[id]|(newout&~v.def[id]);
if(v.out[id]!=newout||v.in[id]!=newin){v.out[id]=newout;
v.in[id]=newin;
changed=1;
}}
 if(++iteration>4097)return 0;
}while(changed);
*out=v;
return 1;
}
#ifdef __cplusplus
}
#endif
#endif
