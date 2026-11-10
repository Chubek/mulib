/* MuLIB compiler tools: Iterative control-flow dominance sets and immediate dominator tree for up to 64 blocks.
 * ISO C11 single-header module. Define MULIB_IMPL_DOMINANCE in ONE translation unit.
 * Bounded operations report failure instead of silently truncating data.
 */
#ifndef MULIB_DOMINANCE_H
#define MULIB_DOMINANCE_H
#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include "mulib-cfg.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "mulib-cfg.h"
typedef struct {uint64_t dom[64],reachable;
size_t idom[64],blocks;
} mulib_dominance_t;
int mulib_dominance_compute(const mulib_cfg_t *g,size_t entry,mulib_dominance_t *out);
int mulib_dominance_test(const mulib_dominance_t *d,size_t a,size_t b);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_DOMINANCE_H */
#if defined(MULIB_IMPL_DOMINANCE) && !defined(MULIB_DOMINANCE_IMPLEMENTED)
#define MULIB_DOMINANCE_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_dominance_compute(const mulib_cfg_t *g,size_t entry,mulib_dominance_t *out){
 if(!g||!out||g->blocks>64||!g->blocks||entry>=g->blocks)return 0;
mulib_dominance_t d;
memset(&d,0,sizeof(d));
 if(!mulib_cfg_reachable(g,entry,&d.reachable))return 0;
d.blocks=g->blocks;
 for(size_t i=0;i<g->blocks;i++){d.dom[i]=(d.reachable&(UINT64_C(1)<<i))?(i==entry?(UINT64_C(1)<<i):d.reachable):0;
d.idom[i]=SIZE_MAX;
}
 int changed;
size_t steps=0;
do{changed=0;
for(size_t b=0;b<g->blocks;b++){if(b==entry||!(d.reachable&(UINT64_C(1)<<b)))continue;
 uint64_t now=d.reachable;
int haspred=0;
for(size_t e=0;e<g->count;e++)if(g->edges[e].to==b&&(d.reachable&(UINT64_C(1)<<g->edges[e].from))){now &= d.dom[g->edges[e].from];
haspred=1;
}
 if(!haspred)return 0;
now|=UINT64_C(1)<<b;
if(now!=d.dom[b]){d.dom[b]=now;
changed=1;
}}
 steps++;
if(steps>65)return 0;
}while(changed);
 for(size_t b=0;b<g->blocks;b++){if(b==entry||!(d.reachable&(UINT64_C(1)<<b)))continue;
uint64_t proper=d.dom[b]&~(UINT64_C(1)<<b);
size_t parent=SIZE_MAX;
 for(size_t cand=0;cand<g->blocks;cand++)if(proper&(UINT64_C(1)<<cand)){
 if((d.dom[cand]&proper)==proper){parent=cand;
break;
}}
 if(parent==SIZE_MAX)return 0;
d.idom[b]=parent;
}
 *out=d;
return 1;
}
int mulib_dominance_test(const mulib_dominance_t *d,size_t a,size_t b){return d&&d->blocks<=64&&a<d->blocks&&b<d->blocks&&((d->dom[b]>>a)&UINT64_C(1));
}
#ifdef __cplusplus
}
#endif
#endif
