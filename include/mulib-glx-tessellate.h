/* MuLIB graphical extension: glx-tessellate. C11, header-only. */
#ifndef MULIB_GLX_TESSELLATE_H
#define MULIB_GLX_TESSELLATE_H
#include "mulib-glx-core.h"
static inline double mulib_glx_tess_cross(mgx_point a,mgx_point b,mgx_point c){return (b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);}
/* Ear clipping of a simple, non-self-intersecting contour; returns triangle count. */
static inline size_t mulib_glx_tessellate(const mgx_point *p,size_t n,uint32_t *out,size_t slots){if(!p||!out||n<3||n>UINT32_MAX||n>SIZE_MAX/sizeof(uint32_t)||slots/3<n-2)return 0;uint32_t *v=(uint32_t*)malloc(n*sizeof *v);if(!v)return 0;double area=0;for(size_t i=0;i<n;i++){v[i]=(uint32_t)i;area+=p[i].x*p[(i+1)%n].y-p[(i+1)%n].x*p[i].y;}if(!isfinite(area)||area==0){free(v);return 0;}double sign=area>0?1:-1;size_t m=n,k=0;while(m>2){int found=0;for(size_t j=0;j<m;j++){uint32_t a=v[(j+m-1)%m],b=v[j],c=v[(j+1)%m];if(sign*mulib_glx_tess_cross(p[a],p[b],p[c])<=0)continue;int inside=0;for(size_t i=0;i<m;i++){uint32_t d=v[i];if(d==a||d==b||d==c)continue;if(sign*mulib_glx_tess_cross(p[a],p[b],p[d])>=0&&sign*mulib_glx_tess_cross(p[b],p[c],p[d])>=0&&sign*mulib_glx_tess_cross(p[c],p[a],p[d])>=0){inside=1;break;}}if(inside)continue;out[k++]=a;out[k++]=b;out[k++]=c;memmove(v+j,v+j+1,(m-j-1)*sizeof *v);--m;found=1;break;}if(!found){free(v);return 0;}}free(v);return k/3;}
#endif
