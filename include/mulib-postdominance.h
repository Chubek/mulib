/* MuLIB compiler tools: Postdominator computation for single-exit reachable control-flow graphs.
 * ISO C11 single-header module. Define MULIB_IMPL_POSTDOMINANCE in ONE translation unit.
 * Bounded operations report failure instead of silently truncating data.
 */
#ifndef MULIB_POSTDOMINANCE_H
#define MULIB_POSTDOMINANCE_H
#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include "mulib-dominance.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "mulib-dominance.h"
int mulib_postdominance_compute(const mulib_cfg_t *g,size_t exit,mulib_dominance_t *out);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_POSTDOMINANCE_H */
#if defined(MULIB_IMPL_POSTDOMINANCE) && !defined(MULIB_POSTDOMINANCE_IMPLEMENTED)
#define MULIB_POSTDOMINANCE_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_postdominance_compute(const mulib_cfg_t *g,size_t exit,mulib_dominance_t *out){
 if(!mulib_cfg_validate(g)||!out||!g->blocks||g->blocks>64||exit>=g->blocks)return 0;
 for(size_t i=0;i<g->count;i++)if(g->edges[i].from==exit)return 0;
mulib_cfg_t reverse={0};
reverse.blocks=g->blocks;
 for(size_t i=0;i<g->count;i++)if(!mulib_cfg_add(&reverse,g->edges[i].to,g->edges[i].from)){mulib_cfg_free(&reverse);
return 0;
}
 int rc=mulib_dominance_compute(&reverse,exit,out);
mulib_cfg_free(&reverse);
return rc;
}
#ifdef __cplusplus
}
#endif
#endif
