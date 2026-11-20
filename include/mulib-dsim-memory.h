/* MuLIB: dsim-memory. C11 single-header kernel.
 * Define MULIB_IMPL_DSIM_MEMORY in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_DSIM_MEMORY_H
#define MULIB_DSIM_MEMORY_H
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <float.h>
#include <limits.h>
#include <ctype.h>
#include <errno.h>
#include "mulib-storage-core.h"
#include "mulib-num-core.h"
#include "mulib-graph-core.h"
#include "mulib-hwil-value.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {mulib_hwil_hw_value *v;size_t depth;unsigned width;} mulib_dsim_sim_memory;
int mulib_dsim_memory_init(mulib_dsim_sim_memory *m,size_t depth,unsigned width);
int mulib_dsim_memory_write(mulib_dsim_sim_memory *m,size_t address,mulib_hwil_hw_value value);
int mulib_dsim_memory_read(const mulib_dsim_sim_memory *m,size_t address,mulib_hwil_hw_value *value);
void mulib_dsim_memory_free(mulib_dsim_sim_memory *m);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_DSIM_MEMORY) && !defined(MULIB_DSIM_MEMORY_H_IMPLEMENTED)
#define MULIB_DSIM_MEMORY_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_dsim_memory_init(mulib_dsim_sim_memory *m,size_t depth,unsigned width){if(!m||m->v||!depth||!width||width>64)return 0;m->v=(mulib_hwil_hw_value*)mulib_storage_alloc(depth,sizeof *m->v);if(!m->v)return 0;m->depth=depth;m->width=width;for(size_t i=0;i<depth;i++)m->v[i].width=width;return 1;}
int mulib_dsim_memory_write(mulib_dsim_sim_memory *m,size_t address,mulib_hwil_hw_value value){if(!m||!m->v||address>=m->depth||value.width!=m->width)return 0;return mulib_hwil_value_make(value.width,value.value,value.known,value.highz,m->v+address);}
int mulib_dsim_memory_read(const mulib_dsim_sim_memory *m,size_t address,mulib_hwil_hw_value *value){if(!m||!m->v||!value||address>=m->depth)return 0;*value=m->v[address];return 1;}
void mulib_dsim_memory_free(mulib_dsim_sim_memory *m){if(m){free(m->v);memset(m,0,sizeof *m);}}
#ifdef __cplusplus
}
#endif
#endif
