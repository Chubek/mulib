/* MuLIB graphical extension: retgtk-tree. C11, header-only. */
#ifndef MULIB_RETGTK_TREE_H
#define MULIB_RETGTK_TREE_H
#include "mulib-retgtk-widget.h"
static inline mulib_retgtk_widget_t *mulib_retgtk_tree_find(mulib_retgtk_widget_t *w,uint64_t id){if(!w)return NULL;if(w->object.id==id)return w;for(size_t i=0;i<w->n;i++){mulib_retgtk_widget_t *r=mulib_retgtk_tree_find(w->children[i],id);if(r)return r;}return NULL;}
static inline mulib_retgtk_widget_t *mulib_retgtk_tree_hit(mulib_retgtk_widget_t *w,double x,double y){if(!mulib_retgtk_widget_is_interactive(w)||!mgx_contains(w->bounds,x,y))return NULL;for(size_t i=w->n;i;i--){mulib_retgtk_widget_t *r=mulib_retgtk_tree_hit(w->children[i-1],x,y);if(r)return r;}return w;}
/* Depth-first tab order. Hidden/disabled subtrees are excluded; wraps in both directions. */
static inline void mret_tree_focus_scan(mulib_retgtk_widget_t *w,mulib_retgtk_widget_t *current,
                                       mulib_retgtk_widget_t **first,mulib_retgtk_widget_t **last,
                                       mulib_retgtk_widget_t **before,mulib_retgtk_widget_t **after,int *seen){
    if(!w||!w->visible||!w->enabled)return;
    if(w->focusable){
        if(!*first)*first=w;
        if(w==current){*before=*last;*seen=1;}
        else if(*seen&&!*after)*after=w;
        *last=w;
    }
    for(size_t i=0;i<w->n;i++)mret_tree_focus_scan(w->children[i],current,first,last,before,after,seen);
}
static inline mulib_retgtk_widget_t *mulib_retgtk_tree_focus_next(mulib_retgtk_widget_t *root,mulib_retgtk_widget_t *current,int backwards){
    mulib_retgtk_widget_t *first=NULL,*last=NULL,*before=NULL,*after=NULL;int seen=0;
    if(!mulib_retgtk_widget_is_interactive(root))return NULL;
    mret_tree_focus_scan(root,current,&first,&last,&before,&after,&seen);
    return backwards?(seen&&before?before:last):(seen&&after?after:first);
}
#endif
