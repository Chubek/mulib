/* MuLIB / Amb3r: spice-l. C11 single-header kernel.
 * Define MULIB_IMPL_SPICE_L in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_SPICE_L_H
#define MULIB_SPICE_L_H
#include "mulib-amb3r-base.h"
#include "mulib-spice-mna.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_spice_l_stamp(ma_mna *m,size_t a,size_t b,size_t branch,double henry);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SPICE_L) && !defined(MULIB_SPICE_L_H_IMPLEMENTED)
#define MULIB_SPICE_L_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_spice_l_stamp(ma_mna *m,size_t a,size_t b,size_t branch,double henry){if(!isfinite(henry)||henry<=0||!mulib_spice_mna_voltage(m,a,b,branch,0))return 0;m->c[branch*m->n+branch]-=henry;return 1;}
#ifdef __cplusplus
}
#endif
#endif
