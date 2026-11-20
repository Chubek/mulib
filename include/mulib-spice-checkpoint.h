/* MuLIB: spice-checkpoint. C11 single-header kernel.
 * Define MULIB_IMPL_SPICE_CHECKPOINT in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_SPICE_CHECKPOINT_H
#define MULIB_SPICE_CHECKPOINT_H
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
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {double time,*state;size_t n;} mulib_spice_checkpoint;
int mulib_spice_checkpoint_save(mulib_spice_checkpoint *c,double time,const double *state,size_t n);
int mulib_spice_checkpoint_restore(const mulib_spice_checkpoint *c,double *time,double *state,size_t n);
void mulib_spice_checkpoint_free(mulib_spice_checkpoint *c);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SPICE_CHECKPOINT) && !defined(MULIB_SPICE_CHECKPOINT_H_IMPLEMENTED)
#define MULIB_SPICE_CHECKPOINT_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_spice_checkpoint_save(mulib_spice_checkpoint *c,double time,const double *state,size_t n){if(!c||!isfinite(time)||!mulib_num_all_finite(state,n))return 0;double *v=(double*)mulib_storage_alloc(n,sizeof(double));if(!v)return 0;if(n)memcpy(v,state,n*sizeof(double));free(c->state);c->state=v;c->n=n;c->time=time;return 1;}
int mulib_spice_checkpoint_restore(const mulib_spice_checkpoint *c,double *time,double *state,size_t n){if(!c||!time||!state||!c->state||n!=c->n)return 0;memcpy(state,c->state,n*sizeof(double));*time=c->time;return 1;}
void mulib_spice_checkpoint_free(mulib_spice_checkpoint *c){if(c){free(c->state);memset(c,0,sizeof *c);}}
#ifdef __cplusplus
}
#endif
#endif
