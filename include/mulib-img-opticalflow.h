/** @file mulib-img-opticalflow.h
 * Exhaustive local motion estimation by block matching.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Integer displacement only; no dense multiscale flow.
 */
#ifndef MULIB_IMG_OPTICALFLOW_H
#define MULIB_IMG_OPTICALFLOW_H
#include "mulib-img-core.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <float.h>
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Integer block matching by mean squared luminance error. Three disjoint
 * scalar outputs, unchanged on failure; nonfinite visited luminance fails. */
static inline bool mulib_img_opticalflow_block(mulib_img_view a,mulib_img_view b,int x,int y,int patch,int search,int *dx,int *dy,double *error){
    if(!mulib_img_same(a,b)||!dx||!dy||!error||x<0||y<0||
       (size_t)x>=a.width||(size_t)y>=a.height||patch<0||patch>16||search<0||search>64||
       !mulib_img_memory_disjoint(dx,1,sizeof *dx,dy,1,sizeof *dy)||
       !mulib_img_memory_disjoint(dx,1,sizeof *dx,error,1,sizeof *error)||
       !mulib_img_memory_disjoint(dy,1,sizeof *dy,error,1,sizeof *error))return false;
    double best=DBL_MAX;
    int bx=0,by=0;
    for(int v=-search;v<=search;v++)for(int u=-search;u<=search;u++){
        double e=0;
        for(int j=-patch;j<=patch;j++)for(int i=-patch;i<=patch;i++){
            float p=mulib_img_luma(mulib_img_get(a,mulib_img_clamp_index((int64_t)x+i,a.width),mulib_img_clamp_index((int64_t)y+j,a.height)));
            float q=mulib_img_luma(mulib_img_get(b,mulib_img_clamp_index((int64_t)x+i+u,b.width),mulib_img_clamp_index((int64_t)y+j+v,b.height)));
            if(!isfinite(p)||!isfinite(q))return false;
            double z=(double)p-q;
            e+=z*z;
        }
        if(e<best||(e==best&&u*u+v*v<bx*bx+by*by)){best=e;bx=u;by=v;}
    }
    if(best==DBL_MAX)return false;
    *dx=bx;*dy=by;*error=best/((2*patch+1)*(2*patch+1));
    return true;
}
#ifdef __cplusplus
}
#endif
#endif
