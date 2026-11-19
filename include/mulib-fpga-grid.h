/* MuLIB / Amb3r: fpga-grid. C11 single-header kernel.
 * Define MULIB_IMPL_FPGA_GRID in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_FPGA_GRID_H
#define MULIB_FPGA_GRID_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_fpga_grid_index(size_t w,size_t h,size_t x,size_t y,size_t *id);
size_t mulib_fpga_grid_neighbors(size_t w,size_t h,size_t id,size_t out[4]);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_FPGA_GRID) && !defined(MULIB_FPGA_GRID_H_IMPLEMENTED)
#define MULIB_FPGA_GRID_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_fpga_grid_index(size_t w,size_t h,size_t x,size_t y,size_t *id){size_t n;if(!w||!h||x>=w||y>=h||!id||!ma_mul(w,h,&n))return 0;*id=y*w+x;return 1;}
size_t mulib_fpga_grid_neighbors(size_t w,size_t h,size_t id,size_t out[4]){size_t n;if(!w||!h||!out||!ma_mul(w,h,&n)||id>=n)return 0;size_t k=0;if(id%w)out[k++]=id-1;if(id%w+1<w)out[k++]=id+1;if(id>=w)out[k++]=id-w;if(id/w+1<h)out[k++]=id+w;return k;}
#ifdef __cplusplus
}
#endif
#endif
