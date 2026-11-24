/* MuLIB graphical extension: immgtk-plot-3d. C11, header-only. */
#ifndef MULIB_IMMGTK_PLOT_3D_H
#define MULIB_IMMGTK_PLOT_3D_H
#include "mulib-immgtk-native.h"
typedef struct{double x,y,z;}mig_vec3;
static inline mgx_point mulib_immgtk_plot_3d_project(mig_vec3 p,double yaw,double pitch,double scale,mgx_point origin){double x=p.x*cos(yaw)-p.z*sin(yaw),z=p.x*sin(yaw)+p.z*cos(yaw),y=p.y*cos(pitch)-z*sin(pitch);return (mgx_point){origin.x+x*scale,origin.y-y*scale};}
static inline void mulib_immgtk_plot_3d_line(mulib_immgtk_native_t *g,const mig_vec3 *p,size_t n,double yaw,double pitch,double scale,mgx_point origin){if(!g||(!p&&n))return;for(size_t i=1;i<n;i++){mgx_drawcmd c={0};c.kind=MGX_LINE;c.clip=g->clip;c.a=mulib_immgtk_plot_3d_project(p[i-1],yaw,pitch,scale,origin);c.b=mulib_immgtk_plot_3d_project(p[i],yaw,pitch,scale,origin);c.width=1;c.color=g->theme.accent;if(!mig_draw_add(g,c))return;}}
#endif
