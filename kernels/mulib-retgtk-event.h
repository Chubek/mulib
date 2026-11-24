/* MuLIB graphical extension: retgtk-event. C11, header-only. */
#ifndef MULIB_RETGTK_EVENT_H
#define MULIB_RETGTK_EVENT_H
#include "mulib-retgtk-tree.h"
typedef struct{
    mulib_retgtk_widget_t *capture,*focus,*hover;
    mret_widget_route capture_link,focus_link,hover_link;
    unsigned capture_button,dispatching;
    int explicit_capture;
    uint64_t capture_serial;
}mulib_retgtk_event_router_t;
/* Legacy owning slot helper. Prefer router APIs: only they register detach links. */
static inline void mret_router_set(mulib_retgtk_widget_t **slot,mulib_retgtk_widget_t *w){
    if(!slot||*slot==w)return;
    if(w&&!mulib_retgtk_object_ref(&w->object))return;
    mulib_retgtk_widget_t *old=*slot;*slot=w;mulib_retgtk_widget_unref(old);
}
static inline int mret_event_valid(mulib_retgtk_widget_t *root,mulib_retgtk_widget_t *w){
    return mulib_retgtk_widget_is_descendant(w,root)&&mulib_retgtk_widget_is_interactive(w);
}
static inline void mulib_retgtk_event_router_validate(mulib_retgtk_event_router_t *r,mulib_retgtk_widget_t *root){
    if(!r)return;
    mret_widget_route_bind(&r->capture_link,&r->capture,mret_event_valid(root,r->capture)?r->capture:NULL,MRET_ROUTE_CAPTURE);
    mret_widget_route_bind(&r->focus_link,&r->focus,mret_event_valid(root,r->focus)&&r->focus->focusable?r->focus:NULL,MRET_ROUTE_FOCUS);
    mret_widget_route_bind(&r->hover_link,&r->hover,mret_event_valid(root,r->hover)?r->hover:NULL,MRET_ROUTE_HOVER);
}
static inline int mulib_retgtk_event_focus(mulib_retgtk_event_router_t *r,mulib_retgtk_widget_t *root,mulib_retgtk_widget_t *w){
    if(!r||(w&&(!mret_event_valid(root,w)||!w->focusable)))return 0;
    return mret_widget_route_bind(&r->focus_link,&r->focus,w,MRET_ROUTE_FOCUS);
}
static inline void mulib_retgtk_event_release_capture(mulib_retgtk_event_router_t *r){
    if(!r)return;
    mret_widget_route_bind(&r->capture_link,&r->capture,NULL,MRET_ROUTE_CAPTURE);
    r->explicit_capture=0;r->capture_serial++;
}
/* Explicit capture lasts until release/removal/disable, not an arbitrary button-up. */
static inline int mulib_retgtk_event_capture(mulib_retgtk_event_router_t *r,mulib_retgtk_widget_t *root,mulib_retgtk_widget_t *w){
    if(!r||!w||!mret_event_valid(root,w))return 0;
    if(!mret_widget_route_bind(&r->capture_link,&r->capture,w,MRET_ROUTE_CAPTURE))return 0;
    r->explicit_capture=1;r->capture_serial++;return 1;
}
static inline int mulib_retgtk_event_focus_next(mulib_retgtk_event_router_t *r,mulib_retgtk_widget_t *root,int backwards){
    if(!r)return 0;
    mulib_retgtk_event_router_validate(r,root);
    mulib_retgtk_widget_t *w=mulib_retgtk_tree_focus_next(root,r->focus,backwards);
    if(!w)return 0;
    return mulib_retgtk_event_focus(r,root,w);
}
static inline int mulib_retgtk_event_dispatch(mulib_retgtk_event_router_t *r,mulib_retgtk_widget_t *root,const mgx_event *e){
    if(!r||!e||r->dispatching>=32)return 0;
    if(root&&!mulib_retgtk_object_ref(&root->object))return 0;
    r->dispatching++;
    mulib_retgtk_event_router_validate(r,root);
    int handled=0;
    if(e->type==MGX_EVENT_FOCUS&&!e->down){
        mulib_retgtk_event_release_capture(r);
        mulib_retgtk_event_focus(r,root,NULL);
        mret_widget_route_bind(&r->hover_link,&r->hover,NULL,MRET_ROUTE_HOVER);
        handled=1;
    }else if(e->type==MGX_EVENT_KEY&&e->down&&e->code==15){
        handled=mulib_retgtk_event_focus_next(r,root,!!(e->mods&1));
    }else{
        mulib_retgtk_widget_t *target=NULL;
        int pointer=e->type==MGX_EVENT_MOTION||e->type==MGX_EVENT_BUTTON;
        if(e->type==MGX_EVENT_KEY||e->type==MGX_EVENT_TEXT)target=r->focus;
        else if(pointer||e->type==MGX_EVENT_SCROLL){
            target=mulib_retgtk_tree_hit(root,e->x,e->y);
            if(e->type==MGX_EVENT_MOTION)
                mret_widget_route_bind(&r->hover_link,&r->hover,target,MRET_ROUTE_HOVER);
            if(e->type==MGX_EVENT_BUTTON&&e->down&&!r->capture){
                /* Focus the nearest eligible ancestor, not just the hit leaf. */
                mulib_retgtk_widget_t *f=target;
                while(f&&!f->focusable)f=f->parent;
                mulib_retgtk_event_focus(r,root,f);
                if(mret_widget_route_bind(&r->capture_link,&r->capture,target,MRET_ROUTE_CAPTURE)){
                    r->capture_button=e->code;r->explicit_capture=0;r->capture_serial++;
                }
            }
            if(pointer&&r->capture)target=r->capture;
        }
        uint64_t serial=r->capture_serial;
        /* Pin the original bubble path before invoking any callbacks. This also
           protects parents when a child detaches itself and destroys its branch. */
        mulib_retgtk_widget_t **path=NULL;size_t n=0,cap=0;
        for(mulib_retgtk_widget_t *w=target;w;w=w->parent){
            if(n==SIZE_MAX||!mgx_grow((void**)&path,&cap,n+1,sizeof *path)||!mulib_retgtk_object_ref(&w->object)){
                mulib_retgtk_widget_children_release(path,n);path=NULL;n=0;break;
            }
            path[n++]=w;
            if(w==root)break;
        }
        for(size_t i=0;i<n;i++){
            mulib_retgtk_widget_t *w=path[i];
            /* Detachment/reparenting cancels delivery along the obsolete path. */
            if(!mret_event_valid(root,path[0])||!mret_event_valid(root,w)||
               (i&&path[i-1]->parent!=w))break;
            if(w->ops&&w->ops->event&&w->ops->event(w,e)){handled=1;break;}
        }
        mulib_retgtk_widget_children_release(path,n);
        if(e->type==MGX_EVENT_BUTTON&&!e->down&&r->capture&&!r->explicit_capture&&
           e->code==r->capture_button&&serial==r->capture_serial)
            mulib_retgtk_event_release_capture(r);
    }
    mulib_retgtk_event_router_validate(r,root);
    r->dispatching--;
    mulib_retgtk_widget_unref(root);
    return handled;
}
static inline void mulib_retgtk_event_router_free(mulib_retgtk_event_router_t *r){
    if(!r)return;
    mulib_retgtk_event_release_capture(r);
    mret_widget_route_bind(&r->focus_link,&r->focus,NULL,MRET_ROUTE_FOCUS);
    mret_widget_route_bind(&r->hover_link,&r->hover,NULL,MRET_ROUTE_HOVER);
}
#endif
