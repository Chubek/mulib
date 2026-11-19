/* MuLIB / Amb3r: spice-k. C11 single-header kernel.
 * Define MULIB_IMPL_SPICE_K in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_SPICE_K_H
#define MULIB_SPICE_K_H
#include "mulib-amb3r-base.h"
#include "mulib-spice-mna.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_spice_k_stamp(ma_mna *m,size_t a,size_t b,double l1,double l2,double k);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SPICE_K) && !defined(MULIB_SPICE_K_H_IMPLEMENTED)
#define MULIB_SPICE_K_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_spice_k_stamp(ma_mna *m,size_t a,size_t b,double l1,double l2,double k){if(!m||!m->c||a>=m->n||b>=m->n||a==b||l1<=0||l2<=0||fabs(k)>1||!isfinite(l1+l2+k))return 0;double mutual=k*sqrt(l1)*sqrt(l2);m->c[a*m->n+b]-=mutual;m->c[b*m->n+a]-=mutual;return 1;}
#ifdef __cplusplus
}
#endif
#endif
