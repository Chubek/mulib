/* MuLIB graphical extension: immgtk-dock. C11, header-only. */
#ifndef MULIB_IMMGTK_DOCK_H
#define MULIB_IMMGTK_DOCK_H
#include "mulib-immgtk-native.h"
typedef struct{int left,right,vertical;double ratio;uint64_t window;mgx_rect bounds;}mig_docknode;
typedef struct{mig_docknode *v;size_t n,cap;}mulib_immgtk_dock_t;
static inline int mulib_immgtk_dock_leaf(mulib_immgtk_dock_t *d,uint64_t id){if(d->n>=INT_MAX||!mgx_grow((void**)&d->v,&d->cap,d->n+1,sizeof *d->v))return -1;int i=(int)d->n++;d->v[i]=(mig_docknode){-1,-1,0,.5,id,{0,0,0,0}};return i;}
static inline int mulib_immgtk_dock_split(mulib_immgtk_dock_t *d,int at,uint64_t window,int vert,double ratio){if(at<0||(size_t)at>=d->n||d->v[at].left>=0||!isfinite(ratio)||d->n>INT_MAX-2||!mgx_grow((void**)&d->v,&d->cap,d->n+2,sizeof *d->v))return 0;int a=mulib_immgtk_dock_leaf(d,d->v[at].window),b=mulib_immgtk_dock_leaf(d,window);d->v[at]=(mig_docknode){a,b,vert,mgx_clamp(ratio,.05,.95),0,{0,0,0,0}};return 1;}
static inline void mulib_immgtk_dock_arrange(mulib_immgtk_dock_t *d,int i,mgx_rect r){if(i<0||(size_t)i>=d->n)return;mig_docknode *n=d->v+i;n->bounds=r;if(n->left<0)return;mgx_rect a=r,b=r;if(n->vertical){a.h=(int)(r.h*n->ratio);b.y+=a.h;b.h-=a.h;}else{a.w=(int)(r.w*n->ratio);b.x+=a.w;b.w-=a.w;}mulib_immgtk_dock_arrange(d,n->left,a);mulib_immgtk_dock_arrange(d,n->right,b);}
static inline void mulib_immgtk_dock_free(mulib_immgtk_dock_t *d){free(d->v);memset(d,0,sizeof *d);}
#endif
