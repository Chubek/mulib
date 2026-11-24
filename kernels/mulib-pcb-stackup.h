/* MuLIB: pcb-stackup. C11 single-header kernel.
 * Define MULIB_IMPL_PCB_STACKUP in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_PCB_STACKUP_H
#define MULIB_PCB_STACKUP_H
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
typedef struct {double thickness,epsilon_r,loss_tangent;unsigned conductive;} mulib_pcb_layer;
int mulib_pcb_stackup_thickness(const mulib_pcb_layer *layers,size_t n,double *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_PCB_STACKUP) && !defined(MULIB_PCB_STACKUP_H_IMPLEMENTED)
#define MULIB_PCB_STACKUP_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_pcb_stackup_thickness(const mulib_pcb_layer *layers,size_t n,double *out){if(!layers||!n||!out)return 0;double total=0;for(size_t i=0;i<n;i++){if(!isfinite(layers[i].thickness)||layers[i].thickness<=0||!isfinite(layers[i].epsilon_r)||layers[i].epsilon_r<1)return 0;total+=layers[i].thickness;}if(!isfinite(total))return 0;*out=total;return 1;}
#ifdef __cplusplus
}
#endif
#endif
