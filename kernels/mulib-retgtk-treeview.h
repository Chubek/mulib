/* MuLIB graphical extension: retgtk-treeview. C11, header-only. */
#ifndef MULIB_RETGTK_TREEVIEW_H
#define MULIB_RETGTK_TREEVIEW_H
#include "mulib-retgtk-treemodel.h"
#include "mulib-retgtk-delegate.h"
static inline void mret_treeview_paint(mulib_retgtk_widget_t *w,mret_paint *p){mulib_retgtk_treemodel_t *m=w->data;if(!m)return;int y=w->bounds.y;for(size_t i=0;i<m->n&&y<w->bounds.y+w->bounds.h;i++){unsigned depth;if(!mulib_retgtk_treemodel_visible(m,i,&depth))continue;mgx_rect r={mgx_int(w->bounds.x+depth*12.),y,w->bounds.w,22};mulib_retgtk_delegate_paint(NULL,p,r,m->v[i].label,w->value>=0&&i==(size_t)w->value);y+=22;}}
static inline int mret_treeview_event(mulib_retgtk_widget_t *w,const mgx_event *e){if(e->type!=MGX_EVENT_BUTTON||e->down||!mgx_contains(w->bounds,e->x,e->y))return 0;mulib_retgtk_treemodel_t *m=w->data;size_t row=(size_t)((e->y-w->bounds.y)/22);for(size_t i=0;m&&i<m->n;i++)if(mulib_retgtk_treemodel_visible(m,i,NULL)){if(row--==0){m->v[i].expanded=!m->v[i].expanded;w->value=(double)i;mulib_retgtk_widget_invalidate(w);return 1;}}return 0;}
static const mulib_retgtk_widget_ops mret_treeview_ops={NULL,NULL,NULL,mret_treeview_paint,mret_treeview_event};
static inline mulib_retgtk_widget_t *mulib_retgtk_treeview(uint64_t id,mulib_retgtk_treemodel_t *m){mulib_retgtk_widget_t *w=mulib_retgtk_widget_create(id,&mret_treeview_ops);if(w){w->data=m;w->value=-1;}return w;}
#endif
