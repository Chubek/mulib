/* MuLIB graphical extension: retgtk-widget. C11, header-only. */
#ifndef MULIB_RETGTK_WIDGET_H
#define MULIB_RETGTK_WIDGET_H
#include "mulib-retgtk-object.h"
#include "mulib-retgtk-signal.h"
#include "mulib-retgtk-type.h"
typedef struct mulib_retgtk_widget mulib_retgtk_widget_t;
typedef struct{mgx_surface *surface;const mulib_glx_font_t *font;mgui_theme theme;}mret_paint;
typedef struct{void (*destroy)(mulib_retgtk_widget_t *);void (*measure)(mulib_retgtk_widget_t *,mulib_gui_constraints_t,mulib_gui_size_t *);void (*arrange)(mulib_retgtk_widget_t *,mgx_rect);void (*paint)(mulib_retgtk_widget_t *,mret_paint *);int (*event)(mulib_retgtk_widget_t *,const mgx_event *);}mulib_retgtk_widget_ops;
typedef struct mret_widget_route {
    mulib_retgtk_widget_t *widget, **slot;
    struct mret_widget_route *next, **prev;
    unsigned kind;
} mret_widget_route;
struct mulib_retgtk_widget{mulib_retgtk_object_t object;const mulib_retgtk_widget_ops *ops;mulib_retgtk_widget_t *parent,**children;size_t n,cap;mulib_retgtk_signal_t signals;mgx_rect bounds;mulib_gui_size_t desired;int visible,enabled,focusable,dirty,pressed,hover,focused;double weight,value,lo,hi;char *text;void *data;uint32_t color;unsigned kind;
    mret_widget_route *routes;
    const mulib_retgtk_type_t *type;
};
enum{MRET_CLICK=1,MRET_CHANGED,MRET_FOCUS,MRET_CLOSE};
static inline void mulib_retgtk_widget_invalidate(mulib_retgtk_widget_t *w){for(;w;w=w->parent)w->dirty=1;}
static inline void mulib_retgtk_widget_unref(mulib_retgtk_widget_t *w){if(w)mulib_retgtk_object_unref(&w->object);}
enum{MRET_ROUTE_CAPTURE=1,MRET_ROUTE_FOCUS,MRET_ROUTE_HOVER};
/* Intrusive route links own one widget ref. They live inside an immovable router. */
static inline void mret_widget_route_clear(mret_widget_route *link){
    if(!link)return;
    mulib_retgtk_widget_t *w=link->widget;
    if(link->prev){*link->prev=link->next;if(link->next)link->next->prev=link->prev;}
    if(link->slot)*link->slot=NULL;
    if(w){
        if(link->kind==MRET_ROUTE_CAPTURE)w->pressed=0;
        if(link->kind==MRET_ROUTE_FOCUS)w->focused=0;
        if(link->kind==MRET_ROUTE_HOVER)w->hover=0;
        mulib_retgtk_widget_invalidate(w);
    }
    memset(link,0,sizeof *link);
    mulib_retgtk_widget_unref(w);
}
static inline int mret_widget_route_bind(mret_widget_route *link,mulib_retgtk_widget_t **slot,mulib_retgtk_widget_t *w,unsigned kind){
    if(!link||!slot)return 0;
    if(link->widget==w&&link->slot==slot&&*slot==w)return 1;
    if(w&&!mulib_retgtk_object_ref(&w->object))return 0;
    /* Also accepts legacy direct slot assignments owning one reference. */
    mulib_retgtk_widget_t *unlinked=link->widget?NULL:*slot;
    mret_widget_route_clear(link);
    *slot=w;
    mulib_retgtk_widget_unref(unlinked);
    if(w){
        link->widget=w;link->slot=slot;link->kind=kind;
        link->next=w->routes;link->prev=&w->routes;
        if(link->next)link->next->prev=&link->next;
        w->routes=link;
        if(kind==MRET_ROUTE_FOCUS)w->focused=1;
        if(kind==MRET_ROUTE_HOVER)w->hover=1;
        mulib_retgtk_widget_invalidate(w);
    }
    return 1;
}
static inline void mret_widget_routes_clear_subtree(mulib_retgtk_widget_t *w){
    /* Caller holds a reference throughout; no user callbacks run while clearing. */
    if(!w)return;
    while(w->routes)mret_widget_route_clear(w->routes);
    w->pressed=w->focused=w->hover=0;
    for(size_t i=0;i<w->n;i++)mret_widget_routes_clear_subtree(w->children[i]);
}
static inline int mulib_retgtk_widget_is_descendant(const mulib_retgtk_widget_t *w,const mulib_retgtk_widget_t *root){
    if(!root)return 0;
    for(;w;w=w->parent)if(w==root)return 1;
    return 0;
}
static inline int mulib_retgtk_widget_is_interactive(const mulib_retgtk_widget_t *w){
    if(!w)return 0;
    for(;w;w=w->parent)if(!w->visible||!w->enabled||!w->object.refs)return 0;
    return 1;
}
static inline void mulib_retgtk_widget_set_visible(mulib_retgtk_widget_t *w,int visible){
    if(!w||!mulib_retgtk_object_ref(&w->object))return;
    w->visible=!!visible;if(!w->visible)mret_widget_routes_clear_subtree(w);
    mulib_retgtk_widget_invalidate(w);mulib_retgtk_widget_unref(w);
}
static inline void mulib_retgtk_widget_set_enabled(mulib_retgtk_widget_t *w,int enabled){
    if(!w||!mulib_retgtk_object_ref(&w->object))return;
    w->enabled=!!enabled;if(!w->enabled)mret_widget_routes_clear_subtree(w);
    mulib_retgtk_widget_invalidate(w);mulib_retgtk_widget_unref(w);
}
static inline void mret_finalize(mulib_retgtk_object_t *o){
    mulib_retgtk_widget_t *w=(mulib_retgtk_widget_t*)o;
    /* Detach before user destruction code can mutate a half-destroyed tree. */
    size_t n=w->n;mulib_retgtk_widget_t **children=w->children;
    w->children=NULL;w->n=w->cap=0;
    for(size_t i=0;i<n;i++){children[i]->parent=NULL;mret_widget_routes_clear_subtree(children[i]);}
    mulib_retgtk_signal_free(&w->signals);
    w->signals.closing=1; /* Destruction must not reconnect to dying storage. */
    if(w->ops&&w->ops->destroy)w->ops->destroy(w);
    for(size_t i=0;i<n;i++)mulib_retgtk_widget_unref(children[i]);
    free(children);free(w->text);free(w);
}
static inline mulib_retgtk_widget_t *mulib_retgtk_widget_create(uint64_t id,const mulib_retgtk_widget_ops *ops){
    mulib_retgtk_widget_t *w=calloc(1,sizeof *w);if(!w)return NULL;
    mulib_retgtk_object_init(&w->object,id,mret_finalize);w->ops=ops;
    mulib_retgtk_signal_bind_owner(&w->signals,&w->object);
    w->visible=w->enabled=w->dirty=1;w->desired=(mulib_gui_size_t){80,24};
    w->weight=1;w->hi=1;w->color=0xffffffff;return w;
}
static inline int mulib_retgtk_widget_set_type(mulib_retgtk_widget_t *w,const mulib_retgtk_type_t *type){
    if(!w||!type||type->size!=sizeof *w||!mulib_retgtk_type_equal(type,type))return 0;
    w->type=type;return 1;
}
static inline int mulib_retgtk_widget_is(const mulib_retgtk_widget_t *w,const mulib_retgtk_type_t *type){
    return w&&mulib_retgtk_type_is(w->type,type);
}
static inline int mulib_retgtk_widget_text(mulib_retgtk_widget_t *w,const char *s){if(!w||!s)return 0;size_t n=strlen(s);char *t=malloc(n+1);if(!t)return 0;memcpy(t,s,n+1);free(w->text);w->text=t;mulib_retgtk_widget_invalidate(w);return 1;}
static inline int mulib_retgtk_widget_add(mulib_retgtk_widget_t *p,mulib_retgtk_widget_t *c){if(!p||!c||!p->object.refs||!c->object.refs||c->parent||p->n==SIZE_MAX)return 0;for(mulib_retgtk_widget_t *w=p;w;w=w->parent)if(w==c)return 0;if(!mgx_grow((void**)&p->children,&p->cap,p->n+1,sizeof c)||!mulib_retgtk_object_ref(&c->object))return 0;p->children[p->n++]=c;c->parent=p;mulib_retgtk_widget_invalidate(p);return 1;}
static inline int mulib_retgtk_widget_remove(mulib_retgtk_widget_t *p,mulib_retgtk_widget_t *c){
    if(!p||!c||!p->object.refs)return 0;
    for(size_t i=0;i<p->n;i++)if(p->children[i]==c){
        memmove(p->children+i,p->children+i+1,(--p->n-i)*sizeof c);
        c->parent=NULL;
        /* Tree ownership is retained until every descendant route is invalidated. */
        mret_widget_routes_clear_subtree(c);
        mulib_retgtk_widget_invalidate(p);mulib_retgtk_widget_unref(c);return 1;
    }
    return 0;
}
/* Snapshot refs protect traversal from sibling removal/reallocation in callbacks.
   New children participate in the next traversal. NULL with n=0 also means OOM. */
static inline mulib_retgtk_widget_t **mulib_retgtk_widget_children_snapshot(mulib_retgtk_widget_t *w,size_t *n){
    if(!n)return NULL;
    *n=0;if(!w||!w->n||w->n>SIZE_MAX/sizeof *w->children)return NULL;
    mulib_retgtk_widget_t **v=malloc(w->n*sizeof *v);if(!v)return NULL;
    for(size_t i=0;i<w->n;i++){
        if(!mulib_retgtk_object_ref(&w->children[i]->object)){
            while(*n)mulib_retgtk_widget_unref(v[--*n]);
            free(v);return NULL;
        }
        v[(*n)++]=w->children[i];
    }
    return v;
}
static inline void mulib_retgtk_widget_children_release(mulib_retgtk_widget_t **v,size_t n){
    for(size_t i=0;i<n;i++)mulib_retgtk_widget_unref(v[i]);
    free(v);
}
static inline mulib_gui_size_t mulib_retgtk_widget_measure(mulib_retgtk_widget_t *w,mulib_gui_constraints_t c){
    mulib_gui_size_t s={0,0};
    if(!w||!mulib_retgtk_object_ref(&w->object))return s;
    if(w->visible){
        s=w->desired;
        if(w->ops&&w->ops->measure)w->ops->measure(w,c,&s);
        if(!w->visible)s=(mulib_gui_size_t){0,0};
        else{s.w=mgx_max(c.min_w,mgx_min(c.max_w,s.w));s.h=mgx_max(c.min_h,mgx_min(c.max_h,s.h));}
    }
    mulib_retgtk_widget_unref(w);return s;
}
static inline void mulib_retgtk_widget_arrange(mulib_retgtk_widget_t *w,mgx_rect b){
    if(!w||!mulib_retgtk_object_ref(&w->object))return;
    w->bounds=b;
    if(w->ops&&w->ops->arrange)w->ops->arrange(w,b);
    else{
        size_t n=0;mulib_retgtk_widget_t **v=mulib_retgtk_widget_children_snapshot(w,&n);
        for(size_t i=0;i<n;i++)if(v[i]->parent==w)mulib_retgtk_widget_arrange(v[i],b);
        mulib_retgtk_widget_children_release(v,n);
    }
    mulib_retgtk_widget_unref(w);
}
static inline void mulib_retgtk_widget_paint(mulib_retgtk_widget_t *w,mret_paint *p){
    if(!w||!w->visible||!p||!p->surface||!mulib_retgtk_object_ref(&w->object))return;
    mgx_rect old=p->surface->clip;p->surface->clip=mgx_intersect(old,w->bounds);
    w->dirty=0;
    if(w->ops&&w->ops->paint)w->ops->paint(w,p);
    size_t n=0;mulib_retgtk_widget_t **v=mulib_retgtk_widget_children_snapshot(w,&n);
    if(w->visible)for(size_t i=0;i<n;i++)if(v[i]->parent==w)mulib_retgtk_widget_paint(v[i],p);
    mulib_retgtk_widget_children_release(v,n);p->surface->clip=old;
    mulib_retgtk_widget_unref(w);
}
static inline void mret_text_paint(mulib_retgtk_widget_t *w,mret_paint *p){if(w->text&&p->font)mulib_glx_text_draw(p->surface,p->font,w->bounds.x+p->theme.padding,w->bounds.y+mgx_max(0,(w->bounds.h-p->theme.font_size)/2),w->text,strlen(w->text),p->theme.font_size,p->theme.text);}
static inline void mret_control_paint(mulib_retgtk_widget_t *w,mret_paint *p){mgx_fill(p->surface,w->bounds,w->pressed?p->theme.active:w->hover?p->theme.hover:p->theme.normal);if(w->focused){mgx_fill(p->surface,(mgx_rect){w->bounds.x,w->bounds.y,w->bounds.w,2},p->theme.accent);}mret_text_paint(w,p);}
static inline int mret_control_event(mulib_retgtk_widget_t *w,const mgx_event *e){if(!e||!mulib_retgtk_widget_is_interactive(w))return 0;int inside=mgx_contains(w->bounds,e->x,e->y);if(e->type==MGX_EVENT_MOTION){w->hover=inside;mulib_retgtk_widget_invalidate(w);return 0;}if(e->type==MGX_EVENT_BUTTON&&e->code==0){if(e->down&&inside){w->pressed=1;mulib_retgtk_widget_invalidate(w);return 1;}if(!e->down&&w->pressed){w->pressed=0;mulib_retgtk_widget_invalidate(w);if(inside)mulib_retgtk_signal_emit(&w->signals,w,MRET_CLICK,NULL);return 1;}}if(e->type==MGX_EVENT_KEY&&e->down&&(e->code==28||e->code==57)&&w->focused){mulib_retgtk_signal_emit(&w->signals,w,MRET_CLICK,NULL);return 1;}return 0;}
static const mulib_retgtk_widget_ops mret_control_ops={NULL,NULL,NULL,mret_control_paint,mret_control_event};
static inline mulib_retgtk_widget_t *mret_control(uint64_t id,const char *label,const mulib_retgtk_widget_ops *ops){mulib_retgtk_widget_t *w=mulib_retgtk_widget_create(id,ops?ops:&mret_control_ops);if(w){w->focusable=1;if(!mulib_retgtk_widget_text(w,label?label:"")){mulib_retgtk_widget_unref(w);return NULL;}}return w;}
#endif
