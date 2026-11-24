/* MuLIB graphical extension: retgtk-split. C11, header-only. */
#ifndef MULIB_RETGTK_SPLIT_H
#define MULIB_RETGTK_SPLIT_H
#include "mulib-retgtk-widget.h"
static inline void mret_split_arrange(mulib_retgtk_widget_t *w,mgx_rect r){if(!w->n)return;if(w->n==1){mulib_retgtk_widget_arrange(w->children[0],r);return;}double t=mgx_clamp(w->value,.05,.95);mgx_rect a=r,b=r;if(w->kind){a.h=(int)(r.h*t);b.y+=a.h;b.h-=a.h;}else{a.w=(int)(r.w*t);b.x+=a.w;b.w-=a.w;}mulib_retgtk_widget_arrange(w->children[0],a);for(size_t i=1;i<w->n;i++)mulib_retgtk_widget_arrange(w->children[i],b);}
static const mulib_retgtk_widget_ops mret_split_ops={NULL,NULL,mret_split_arrange,NULL,NULL};
static inline mulib_retgtk_widget_t *mulib_retgtk_split(uint64_t id,int vertical,double ratio){mulib_retgtk_widget_t *w=mulib_retgtk_widget_create(id,&mret_split_ops);if(w){w->kind=vertical!=0;w->value=isfinite(ratio)?mgx_clamp(ratio,.05,.95):.5;}return w;}
static inline void mulib_retgtk_split_ratio(mulib_retgtk_widget_t *w,double ratio){if(isfinite(ratio)){w->value=mgx_clamp(ratio,.05,.95);mulib_retgtk_widget_invalidate(w);}}
#endif
