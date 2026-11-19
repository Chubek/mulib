/* MuLIB / Amb3r: spice-behavioral. C11 single-header kernel.
 * Define MULIB_IMPL_SPICE_BEHAVIORAL in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_SPICE_BEHAVIORAL_H
#define MULIB_SPICE_BEHAVIORAL_H
#include "mulib-amb3r-base.h"
#include "mulib-spice-mna.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_spice_behavioral_current(ma_mna *m,size_t a,size_t b,const size_t *controls,const double *derivatives,const double *values,size_t n,double current);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SPICE_BEHAVIORAL) && !defined(MULIB_SPICE_BEHAVIORAL_H_IMPLEMENTED)
#define MULIB_SPICE_BEHAVIORAL_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_spice_behavioral_current(ma_mna *m,size_t a,size_t b,const size_t *controls,const double *derivatives,const double *values,size_t n,double current){if(!ma_mna_nodes(m,a,b)||(!controls&&n)||(!derivatives&&n)||(!values&&n)||!isfinite(current))return 0;double eq=current;for(size_t i=0;i<n;i++){if(controls[i]>=m->n||!isfinite(derivatives[i])||!isfinite(values[i]))return 0;eq-=derivatives[i]*values[i];}if(!isfinite(eq))return 0;for(size_t i=0;i<n;i++){if(a!=MA_NONE)m->g[a*m->n+controls[i]]+=derivatives[i];if(b!=MA_NONE)m->g[b*m->n+controls[i]]-=derivatives[i];}return mulib_spice_mna_current(m,a,b,eq);}
#ifdef __cplusplus
}
#endif
#endif
