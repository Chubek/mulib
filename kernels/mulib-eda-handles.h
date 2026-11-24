/* MuLIB: eda-handles. C11 single-header kernel.
 * Define MULIB_IMPL_EDA_HANDLES in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_EDA_HANDLES_H
#define MULIB_EDA_HANDLES_H
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
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {uint32_t generation; unsigned char live;} mulib_eda_handle_slot;
typedef struct {mulib_eda_handle_slot *slots; size_t n,cap;} mulib_eda_handle_pool;
int mulib_eda_handles_alloc(mulib_eda_handle_pool *p, uint64_t *out);
int mulib_eda_handles_valid(const mulib_eda_handle_pool *p,uint64_t h);
int mulib_eda_handles_release(mulib_eda_handle_pool *p,uint64_t h);
void mulib_eda_handles_free(mulib_eda_handle_pool *p);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_EDA_HANDLES) && !defined(MULIB_EDA_HANDLES_H_IMPLEMENTED)
#define MULIB_EDA_HANDLES_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_eda_handles_alloc(mulib_eda_handle_pool *p, uint64_t *out){if(!p||!out)return 0;size_t i=0;for(;i<p->n;i++)if(!p->slots[i].live&&p->slots[i].generation<UINT32_MAX)break;if(i==p->n){if(i>=UINT32_MAX)return 0;mulib_eda_handle_slot s={1,0};if(!mulib_storage_push((void**)&p->slots,&p->n,&p->cap,sizeof s,&s))return 0;}p->slots[i].live=1;*out=((uint64_t)p->slots[i].generation<<32)|(uint64_t)(i+1);return 1;}
int mulib_eda_handles_valid(const mulib_eda_handle_pool *p,uint64_t h){uint32_t i=(uint32_t)h,g=(uint32_t)(h>>32);return p&&i&&i<=p->n&&p->slots[i-1].live&&p->slots[i-1].generation==g;}
int mulib_eda_handles_release(mulib_eda_handle_pool *p,uint64_t h){if(!mulib_eda_handles_valid(p,h))return 0;mulib_eda_handle_slot *s=&p->slots[(uint32_t)h-1];s->live=0;if(s->generation<UINT32_MAX)s->generation++;return 1;}
void mulib_eda_handles_free(mulib_eda_handle_pool *p){if(p){free(p->slots);memset(p,0,sizeof *p);}}
#ifdef __cplusplus
}
#endif
#endif
