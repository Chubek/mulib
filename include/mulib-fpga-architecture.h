/* MuLIB / Amb3r: fpga-architecture. C11 single-header kernel.
 * Define MULIB_IMPL_FPGA_ARCHITECTURE in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_FPGA_ARCHITECTURE_H
#define MULIB_FPGA_ARCHITECTURE_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {size_t width,height,luts_per_tile,routing_tracks;unsigned lut_inputs;} ma_fpga_arch;
int mulib_fpga_architecture_validate(const ma_fpga_arch *a);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_FPGA_ARCHITECTURE) && !defined(MULIB_FPGA_ARCHITECTURE_H_IMPLEMENTED)
#define MULIB_FPGA_ARCHITECTURE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_fpga_architecture_validate(const ma_fpga_arch *a){if(!a||!a->width||!a->height||!a->lut_inputs||a->lut_inputs>16||!a->luts_per_tile||!a->routing_tracks)return 0;size_t t;return ma_mul(a->width,a->height,&t)&&ma_mul(t,a->luts_per_tile,&t);}
#ifdef __cplusplus
}
#endif
#endif
