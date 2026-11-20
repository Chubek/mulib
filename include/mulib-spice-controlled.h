/* MuLIB: spice-controlled. C11 single-header kernel.
 * Define MULIB_IMPL_SPICE_CONTROLLED in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_SPICE_CONTROLLED_H
#define MULIB_SPICE_CONTROLLED_H
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
#include "mulib-graph-core.h"
#include "mulib-spice-mna.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_spice_vccs(mulib_spice_mna *m,size_t a,size_t b,size_t p,size_t q,double gain);
int mulib_spice_vcvs(mulib_spice_mna *m,size_t a,size_t b,size_t p,size_t q,size_t branch,double gain);
int mulib_spice_cccs(mulib_spice_mna *m,size_t a,size_t b,size_t control,double gain);
int mulib_spice_ccvs(mulib_spice_mna *m,size_t a,size_t b,size_t branch,size_t control,double gain);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SPICE_CONTROLLED) && !defined(MULIB_SPICE_CONTROLLED_H_IMPLEMENTED)
#define MULIB_SPICE_CONTROLLED_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_spice_vccs(mulib_spice_mna *m,size_t a,size_t b,size_t p,size_t q,double gain){if(!mulib_spice_mna_nodes(m,a,b)||!mulib_spice_mna_nodes(m,p,q)||!isfinite(gain))return 0;if(a!=MULIB_STORAGE_NONE&&p!=MULIB_STORAGE_NONE)m->g[a*m->n+p]+=gain;if(a!=MULIB_STORAGE_NONE&&q!=MULIB_STORAGE_NONE)m->g[a*m->n+q]-=gain;if(b!=MULIB_STORAGE_NONE&&p!=MULIB_STORAGE_NONE)m->g[b*m->n+p]-=gain;if(b!=MULIB_STORAGE_NONE&&q!=MULIB_STORAGE_NONE)m->g[b*m->n+q]+=gain;return 1;}
int mulib_spice_vcvs(mulib_spice_mna *m,size_t a,size_t b,size_t p,size_t q,size_t branch,double gain){if(!mulib_spice_mna_nodes(m,p,q)||!isfinite(gain)||!mulib_spice_mna_voltage(m,a,b,branch,0))return 0;if(p!=MULIB_STORAGE_NONE)m->g[branch*m->n+p]-=gain;if(q!=MULIB_STORAGE_NONE)m->g[branch*m->n+q]+=gain;return 1;}
int mulib_spice_cccs(mulib_spice_mna *m,size_t a,size_t b,size_t control,double gain){if(!mulib_spice_mna_nodes(m,a,b)||control>=m->n||!isfinite(gain))return 0;if(a!=MULIB_STORAGE_NONE)m->g[a*m->n+control]+=gain;if(b!=MULIB_STORAGE_NONE)m->g[b*m->n+control]-=gain;return 1;}
int mulib_spice_ccvs(mulib_spice_mna *m,size_t a,size_t b,size_t branch,size_t control,double gain){if(!m||control>=m->n||control==branch||!isfinite(gain)||!mulib_spice_mna_voltage(m,a,b,branch,0))return 0;m->g[branch*m->n+control]-=gain;return 1;}
#ifdef __cplusplus
}
#endif
#endif
