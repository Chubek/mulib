/* MuLIB: fpga-place. C11 single-header kernel.
 * Define MULIB_IMPL_FPGA_PLACE in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_FPGA_PLACE_H
#define MULIB_FPGA_PLACE_H
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
#include "mulib-geom2-core.h"
#include "mulib-graph-core.h"
#include "mulib-fpga-architecture.h"
#include "mulib-place-legalize.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_fpga_place(const mulib_fpga_arch *a,const mulib_geom2_point *desired,size_t n,const unsigned char *blocked,size_t *locations);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_FPGA_PLACE) && !defined(MULIB_FPGA_PLACE_H_IMPLEMENTED)
#define MULIB_FPGA_PLACE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_fpga_place(const mulib_fpga_arch *a,const mulib_geom2_point *desired,size_t n,const unsigned char *blocked,size_t *locations){if(!mulib_fpga_architecture_validate(a))return 0;size_t m=a->width*a->height;mulib_geom2_point *sites=(mulib_geom2_point*)mulib_storage_alloc(m,sizeof *sites);if(!sites)return 0;for(size_t i=0;i<m;i++)sites[i]=(mulib_geom2_point){(double)(i%a->width),(double)(i/a->width)};int ok=mulib_place_legalize(desired,n,sites,m,blocked,locations);free(sites);return ok;}
#ifdef __cplusplus
}
#endif
#endif
