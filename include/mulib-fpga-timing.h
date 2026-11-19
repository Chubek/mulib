/* MuLIB / Amb3r: fpga-timing. C11 single-header kernel.
 * Define MULIB_IMPL_FPGA_TIMING in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_FPGA_TIMING_H
#define MULIB_FPGA_TIMING_H
#include "mulib-amb3r-base.h"
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
double mulib_fpga_timing_path(const double *cell_delays,size_t cells,const double *wire_delays,size_t wires){if(!ma_finite(cell_delays,cells)||!ma_finite(wire_delays,wires))return NAN;double d=0;for(size_t i=0;i<cells;i++){if(cell_delays[i]<0)return NAN;d+=cell_delays[i];}for(size_t i=0;i<wires;i++){if(wire_delays[i]<0)return NAN;d+=wire_delays[i];}return d;}
#ifdef __cplusplus
}
#endif
#endif
