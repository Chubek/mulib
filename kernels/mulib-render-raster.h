/** @file mulib-render-raster.h
 * Screen-space Gouraud triangle rasterization with depth test.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: No clip space, perspective correction or top-left fill rule.
 */
#ifndef MULIB_RENDER_RASTER_H
#define MULIB_RENDER_RASTER_H
#include "mulib-mesh-core.h"
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
typedef struct {double x,y,z;mulib_img_rgba color;} mulib_render_raster_vertex;
static inline double mulib_render_raster_edge(mulib_render_raster_vertex a,mulib_render_raster_vertex b,double x,double y){return (b.x-a.x)*(y-a.y)-(b.y-a.y)*(x-a.x);}
static inline bool mulib_render_raster_triangle(mulib_img_view img,float *depth,size_t cap,const mulib_render_raster_vertex v[3]){size_t n;if(!mulib_img_valid(img)||!depth||!v||!mulib_media_size_mul(img.width,img.height,&n)||cap<n)return false;for(int i=0;i<3;i++)if(!isfinite(v[i].x)||!isfinite(v[i].y)||!isfinite(v[i].z))return false;double a=mulib_render_raster_edge(v[0],v[1],v[2].x,v[2].y);if(!isfinite(a)||fabs(a)<1e-12)return false;double x0=fmax(0,floor(fmin(v[0].x,fmin(v[1].x,v[2].x)))),y0=fmax(0,floor(fmin(v[0].y,fmin(v[1].y,v[2].y)))),x1=fmin(img.width,ceil(fmax(v[0].x,fmax(v[1].x,v[2].x)))),y1=fmin(img.height,ceil(fmax(v[0].y,fmax(v[1].y,v[2].y))));if(x0>=x1||y0>=y1)return true;for(size_t y=(size_t)y0;y<(size_t)y1;y++)for(size_t x=(size_t)x0;x<(size_t)x1;x++){double u=mulib_render_raster_edge(v[1],v[2],x+.5,y+.5)/a,w=mulib_render_raster_edge(v[2],v[0],x+.5,y+.5)/a,t=1-u-w;if(u<0||w<0||t<0)continue;double z=u*v[0].z+w*v[1].z+t*v[2].z;if(z<depth[y*img.width+x]){depth[y*img.width+x]=(float)z;mulib_img_rgba c={(float)(u*v[0].color.r+w*v[1].color.r+t*v[2].color.r),(float)(u*v[0].color.g+w*v[1].color.g+t*v[2].color.g),(float)(u*v[0].color.b+w*v[1].color.b+t*v[2].color.b),(float)(u*v[0].color.a+w*v[1].color.a+t*v[2].color.a)};mulib_img_put(img,x,y,c);}}return true;}
#ifdef __cplusplus
}
#endif
#endif
