/* MuLIB: hwil-memory. C11 single-header kernel.
 * Define MULIB_IMPL_HWIL_MEMORY in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_HWIL_MEMORY_H
#define MULIB_HWIL_MEMORY_H
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
#include "mulib-hwil-context.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {uint64_t *words;size_t depth;unsigned width;} mulib_hwil_hw_memory;
int mulib_hwil_memory_init(mulib_hwil_hw_memory *m,size_t depth,unsigned width);
int mulib_hwil_memory_read(const mulib_hwil_hw_memory *m,size_t addr,uint64_t *out);
int mulib_hwil_memory_write(mulib_hwil_hw_memory *m,size_t addr,uint64_t data,uint64_t enable);
void mulib_hwil_memory_free(mulib_hwil_hw_memory *m);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_HWIL_MEMORY) && !defined(MULIB_HWIL_MEMORY_H_IMPLEMENTED)
#define MULIB_HWIL_MEMORY_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_hwil_memory_init(mulib_hwil_hw_memory *m,size_t depth,unsigned width){if(!m||m->words||!depth||!width||width>64)return 0;uint64_t *v=(uint64_t*)mulib_storage_alloc(depth,sizeof *v);if(!v)return 0;*m=(mulib_hwil_hw_memory){v,depth,width};return 1;}
int mulib_hwil_memory_read(const mulib_hwil_hw_memory *m,size_t addr,uint64_t *out){if(!m||!m->words||addr>=m->depth||!out)return 0;*out=m->words[addr];return 1;}
int mulib_hwil_memory_write(mulib_hwil_hw_memory *m,size_t addr,uint64_t data,uint64_t enable){if(!m||!m->words||addr>=m->depth)return 0;enable&=mulib_hwil_hw_mask(m->width);m->words[addr]=(m->words[addr]&~enable)|(data&enable);return 1;}
void mulib_hwil_memory_free(mulib_hwil_hw_memory *m){if(m){free(m->words);memset(m,0,sizeof *m);}}
#ifdef __cplusplus
}
#endif
#endif
