/* MuLIB / Amb3r: fpga-place. C11 single-header kernel.
 * Define MULIB_IMPL_FPGA_PLACE in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_FPGA_PLACE_H
#define MULIB_FPGA_PLACE_H
#include "mulib-amb3r-base.h"
#include "mulib-fpga-architecture.h"
#include "mulib-place-legalize.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_fpga_place(const ma_fpga_arch *a,const ma_point *desired,size_t n,const unsigned char *blocked,size_t *locations);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_FPGA_PLACE) && !defined(MULIB_FPGA_PLACE_H_IMPLEMENTED)
#define MULIB_FPGA_PLACE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_fpga_place(const ma_fpga_arch *a,const ma_point *desired,size_t n,const unsigned char *blocked,size_t *locations){if(!mulib_fpga_architecture_validate(a))return 0;size_t m=a->width*a->height;ma_point *sites=(ma_point*)ma_alloc(m,sizeof *sites);if(!sites)return 0;for(size_t i=0;i<m;i++)sites[i]=(ma_point){(double)(i%a->width),(double)(i/a->width)};int ok=mulib_place_legalize(desired,n,sites,m,blocked,locations);free(sites);return ok;}
#ifdef __cplusplus
}
#endif
#endif
