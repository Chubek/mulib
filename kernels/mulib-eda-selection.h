/* MuLIB: eda-selection. C11 single-header kernel.
 * Define MULIB_IMPL_EDA_SELECTION in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_EDA_SELECTION_H
#define MULIB_EDA_SELECTION_H
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
typedef struct {uint64_t *v;size_t n,cap;} mulib_eda_selection;
int mulib_eda_selection_set(mulib_eda_selection *s,uint64_t id,int selected);
void mulib_eda_selection_free(mulib_eda_selection *s);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_EDA_SELECTION) && !defined(MULIB_EDA_SELECTION_H_IMPLEMENTED)
#define MULIB_EDA_SELECTION_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_eda_selection_set(mulib_eda_selection *s,uint64_t id,int selected){if(!s||!id)return 0;for(size_t i=0;i<s->n;i++)if(s->v[i]==id){if(!selected){memmove(s->v+i,s->v+i+1,(s->n-i-1)*sizeof id);s->n--;}return 1;}return !selected||mulib_storage_push((void**)&s->v,&s->n,&s->cap,sizeof id,&id);}
void mulib_eda_selection_free(mulib_eda_selection *s){if(s){free(s->v);memset(s,0,sizeof *s);}}
#ifdef __cplusplus
}
#endif
#endif
