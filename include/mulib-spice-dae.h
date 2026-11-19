/* MuLIB / Amb3r: spice-dae. C11 single-header kernel.
 * Define MULIB_IMPL_SPICE_DAE in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_SPICE_DAE_H
#define MULIB_SPICE_DAE_H
#include "mulib-amb3r-base.h"
#include "mulib-spice-mna.h"
#include "mulib-num-dae.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_spice_dae_step(const ma_mna *m,const double *previous,double dt,double *next);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SPICE_DAE) && !defined(MULIB_SPICE_DAE_H_IMPLEMENTED)
#define MULIB_SPICE_DAE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_spice_dae_step(const ma_mna *m,const double *previous,double dt,double *next){if(!m)return 0;return mulib_num_dae_step(m->n,m->c,m->g,m->b,previous,dt,next);}
#ifdef __cplusplus
}
#endif
#endif
