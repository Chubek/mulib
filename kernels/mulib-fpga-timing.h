/* MuLIB: fpga-timing. C11 single-header kernel.
 * Define MULIB_IMPL_FPGA_TIMING in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_FPGA_TIMING_H
#define MULIB_FPGA_TIMING_H
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
#include "mulib-num-core.h"
#ifdef __cplusplus
extern "C" {
#endif

double mulib_fpga_timing_path(const double *cell_delays,size_t cells,const double *wire_delays,size_t wires);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_FPGA_TIMING) && !defined(MULIB_FPGA_TIMING_H_IMPLEMENTED)
#define MULIB_FPGA_TIMING_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
double mulib_fpga_timing_path(const double *cell_delays,size_t cells,const double *wire_delays,size_t wires){if(!mulib_num_all_finite(cell_delays,cells)||!mulib_num_all_finite(wire_delays,wires))return NAN;double d=0;for(size_t i=0;i<cells;i++){if(cell_delays[i]<0)return NAN;d+=cell_delays[i];}for(size_t i=0;i<wires;i++){if(wire_delays[i]<0)return NAN;d+=wire_delays[i];}return d;}
#ifdef __cplusplus
}
#endif
#endif
