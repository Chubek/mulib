/** @file mulib-color-lut3d.h
 * Trilinear and tetrahedral 3D LUT evaluation, red-fastest order.
 * C11 header-only; caller owns buffers unless expressly stated.
 */
#ifndef MULIB_COLOR_LUT3D_H
#define MULIB_COLOR_LUT3D_H
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
typedef struct {const float *rgb;size_t edge,count;} mulib_color_lut3d;
/* count is accessible floats (RGB triples, red-fastest). Finite table entries
 * required; invalid tables/nonfinite RGB return the input pixel unchanged. */
static inline bool mulib_color_lut3d_valid(mulib_color_lut3d t){size_t n,bytes;return t.rgb&&t.edge>=2&&t.edge<=256&&mulib_media_size_mul(t.edge,t.edge,&n)&&mulib_media_size_mul(n,t.edge,&n)&&mulib_media_size_mul(n,3,&n)&&n<=t.count&&mulib_img_memory_span(t.rgb,n,sizeof(float),&bytes);}
static inline mulib_img_rgba mulib_color_lut3d_eval(mulib_color_lut3d t,mulib_img_rgba c,bool tetra){if(!mulib_color_lut3d_valid(t)||!isfinite(c.r)||!isfinite(c.g)||!isfinite(c.b))return c;float x[3]={c.r,c.g,c.b},f[3];size_t i[3];for(int a=0;a<3;a++){double v=mulib_media_clampf(x[a],0,1)*(double)(t.edge-1);i[a]=(size_t)v;if(i[a]==t.edge-1)i[a]--;f[a]=(float)(v-i[a]);}float z[3]={0};if(tetra){int order[3]={0,1,2};for(int a=0;a<2;a++)for(int b=a+1;b<3;b++)if(f[order[a]]<f[order[b]]){int k=order[a];order[a]=order[b];order[b]=k;}unsigned mask=0;for(int j=0;j<4;j++){float w=j==0?1-f[order[0]]:j==3?f[order[2]]:f[order[j-1]]-f[order[j]];size_t off=3*(i[0]+(mask&1)+t.edge*(i[1]+((mask>>1)&1)+t.edge*(i[2]+((mask>>2)&1))));for(int k=0;k<3;k++)z[k]+=w*t.rgb[off+k];if(j<3)mask|=1u<<order[j];}}else for(unsigned mask=0;mask<8;mask++){float w=1;for(int k=0;k<3;k++)w*=mask&(1u<<k)?f[k]:1-f[k];size_t off=3*(i[0]+(mask&1)+t.edge*(i[1]+((mask>>1)&1)+t.edge*(i[2]+((mask>>2)&1))));for(int k=0;k<3;k++)z[k]+=w*t.rgb[off+k];}c.r=z[0];c.g=z[1];c.b=z[2];return c;}
#ifdef __cplusplus
}
#endif
#endif
