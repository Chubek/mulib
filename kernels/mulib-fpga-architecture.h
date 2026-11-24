/* MuLIB: fpga-architecture. C11 single-header kernel.
 * Define MULIB_IMPL_FPGA_ARCHITECTURE in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_FPGA_ARCHITECTURE_H
#define MULIB_FPGA_ARCHITECTURE_H
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
typedef struct {size_t width,height,luts_per_tile,routing_tracks;unsigned lut_inputs;} mulib_fpga_arch;
int mulib_fpga_architecture_validate(const mulib_fpga_arch *a);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_FPGA_ARCHITECTURE) && !defined(MULIB_FPGA_ARCHITECTURE_H_IMPLEMENTED)
#define MULIB_FPGA_ARCHITECTURE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_fpga_architecture_validate(const mulib_fpga_arch *a){if(!a||!a->width||!a->height||!a->lut_inputs||a->lut_inputs>16||!a->luts_per_tile||!a->routing_tracks)return 0;size_t t;return mulib_storage_size_mul(a->width,a->height,&t)&&mulib_storage_size_mul(t,a->luts_per_tile,&t);}
#ifdef __cplusplus
}
#endif
#endif
