/* MuLIB graphical extension: glx-mesh. C11, header-only. */
#ifndef MULIB_GLX_MESH_H
#define MULIB_GLX_MESH_H
#include "mulib-glx-raster.h"
typedef struct {mgx_point *vertices;uint32_t *indices;size_t nv,ni,vc,ic;} mulib_glx_mesh_t;
static inline int mulib_glx_mesh_vertex(mulib_glx_mesh_t *m,mgx_point p){if(!isfinite(p.x)||!isfinite(p.y)||m->nv>=UINT32_MAX||!mgx_grow((void**)&m->vertices,&m->vc,m->nv+1,sizeof p))return 0;m->vertices[m->nv++]=p;return 1;}
static inline int mulib_glx_mesh_triangle(mulib_glx_mesh_t *m,uint32_t a,uint32_t b,uint32_t c){if(a>=m->nv||b>=m->nv||c>=m->nv||!mgx_grow((void**)&m->indices,&m->ic,m->ni+3,4))return 0;m->indices[m->ni++]=a;m->indices[m->ni++]=b;m->indices[m->ni++]=c;return 1;}
static inline void mulib_glx_mesh_draw(mgx_surface *s,const mulib_glx_mesh_t *m,uint32_t c){for(size_t i=0;i+2<m->ni;i+=3){mulib_glx_path_t p={0};for(int j=0;j<3;j++){mgx_point v=m->vertices[m->indices[i+j]];mulib_glx_path_add(&p,v.x,v.y,j==0);}mulib_glx_raster_fill(s,&p,c,0,1);mulib_glx_path_free(&p);}}
static inline void mulib_glx_mesh_free(mulib_glx_mesh_t *m){free(m->vertices);free(m->indices);memset(m,0,sizeof *m);}
#endif
