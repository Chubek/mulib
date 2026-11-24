/* MuLIB notebook desktop extension. C11, header-only.
 * SPDX-License-Identifier: MIT
 * See AGENTS.md for integration and coverage limits. */
#ifndef MULIB_WL_XDGSHELL_H
#define MULIB_WL_XDGSHELL_H
#include "mulib-wl-surface.h"
typedef struct {uint32_t serial;int32_t width,height;unsigned states;} mulib_wl_configure;
typedef struct {mulib_wl_surface *surface;mulib_wl_configure *v;size_t n,cap,limit;uint32_t last_ack;int acknowledged;} mulib_wl_xdgshell;
static inline int mulib_wl_xdg_init(mulib_wl_xdgshell *x,mulib_wl_surface *s,int popup,size_t limit){if(!x||!limit||!mulib_wl_surface_role(s,popup?MULIB_WL_ROLE_POPUP:MULIB_WL_ROLE_TOPLEVEL))return 0;*x=(mulib_wl_xdgshell){.surface=s,.limit=limit};return 1;}
static inline int mulib_wl_xdg_configure(mulib_wl_xdgshell *x,mulib_wl_configure v){if(!x||!v.serial||v.width<0||v.height<0||x->n>=x->limit||x->n==SIZE_MAX||v.serial==x->last_ack)return 0;for(size_t i=0;i<x->n;i++)if(x->v[i].serial==v.serial)return 0;if(!mulib_wm_grow((void**)&x->v,&x->cap,x->n+1,sizeof v))return 0;x->v[x->n++]=v;return 1;}
static inline int mulib_wl_xdg_ack(mulib_wl_xdgshell *x,uint32_t serial,mulib_wl_configure *out){if(x)for(size_t i=0;i<x->n;i++)if(x->v[i].serial==serial){if(out)*out=x->v[i];memmove(x->v,x->v+i+1,(x->n-i-1)*sizeof *x->v);x->n-=i+1;x->last_ack=serial;x->acknowledged=1;x->surface->configured=1;return 1;}return 0;}
static inline void mulib_wl_xdg_free(mulib_wl_xdgshell *x){if(x){free(x->v);memset(x,0,sizeof *x);}}

#include "mulib-wl-compositor.h"
/* xdg_wm_base version 1, toplevels only. Positioners/popups and interactive
 * moves/resizes require policy integration; unsupported requests fail closed. */
typedef struct {uint32_t serial;} mulib_wl_xdg_server;
typedef struct {mulib_wl_xdg_server *owner;unsigned live,refs;} mulib_wl_xdg_base_resource;
typedef struct {mulib_wl_xdg_base_resource *base;mulib_wl_surface_resource *wl;mulib_wl_xdgshell state;uint32_t id,top;unsigned refs,initial_sent,requested,role_created;char *title,*app_id;mulib_wm_rect geometry;int32_t min_w,min_h,max_w,max_h;} mulib_wl_xdg_resource;
static inline void mulib_wl_xdg_base_unref(void *u){mulib_wl_xdg_base_resource *b=(mulib_wl_xdg_base_resource*)u;if(b&&b->refs&&!--b->refs)free(b);}
static inline void mulib_wl_xdg_detach(void *u){mulib_wl_xdg_resource *x=(mulib_wl_xdg_resource*)u;x->wl=NULL;x->state.surface=NULL;}
static inline void mulib_wl_xdg_resource_unref(void *u)
{
    mulib_wl_xdg_resource *x=(mulib_wl_xdg_resource*)u;
    if(!x||!x->refs||--x->refs)return;
    if(x->wl) {
        x->wl->role_user=NULL;
        x->wl->role_commit=NULL;
        x->wl->role_detach=NULL;
        x->wl->role_attach=NULL;
    }
    x->base->live--;
    mulib_wl_xdg_base_unref(x->base);
    mulib_wl_xdg_free(&x->state);
    free(x->title);
    free(x->app_id);
    free(x);
}

static inline void mulib_wl_xdg_reset_mapping(mulib_wl_xdg_resource *x)
{
    if(x->wl)x->wl->surface->configured=0;
    x->initial_sent=0;
    x->state.n=0;
    x->state.last_ack=0;
    x->state.acknowledged=0;
    free(x->title);
    free(x->app_id);
    x->title=x->app_id=NULL;
    x->requested=0;
    x->min_w=x->min_h=x->max_w=x->max_h=0;
}

static inline void mulib_wl_xdg_top_destroy(void *u)
{
    mulib_wl_xdg_resource *x=(mulib_wl_xdg_resource*)u;
    x->top=0;
    if(x->wl) {
        mulib_wl_surface_resource *s=x->wl;
        mulib_wl_shm_resource_unref(s->pending);
        s->pending=NULL;
        s->attach=0;
        if(s->surface->current.buffer||s->surface->attach) {
            (void)mulib_wl_surface_attach(s->surface,NULL,0,0);
            (void)mulib_wl_surface_commit(s->surface);
        }
    }
    mulib_wl_xdg_reset_mapping(x);
    mulib_wl_xdg_resource_unref(x);
}
/* states bit 0=maximized,1=fullscreen,2=resizing,3=activated; encode enum array. */
static inline int mulib_wl_xdg_send_configure(mulib_wl_client *c,mulib_wl_xdg_resource *x,int32_t w,int32_t h,unsigned states){if(!c||!x||!x->top||!x->wl||w<0||h<0||(states&~15u))return 0;uint32_t serial=++x->base->owner->serial;if(!serial)serial=++x->base->owner->serial;uint32_t p[7]={(uint32_t)w,(uint32_t)h,0};size_t n=3;for(unsigned i=0;i<4;i++)if(states&(1u<<i))p[n++]=i+1;p[2]=(uint32_t)((n-3)*4);if(!mulib_wl_xdg_configure(&x->state,(mulib_wl_configure){serial,w,h,states}))return 0;if(!mulib_wl_client_event(c,x->top,0,p,n*4)||!mulib_wl_client_event(c,x->id,0,&serial,4)){c->failed=1;return 0;}x->initial_sent=1;return 1;}
static inline int mulib_wl_xdg_role_attach(void *u,int attaching)
{
    const mulib_wl_xdg_resource *x=(const mulib_wl_xdg_resource*)u;
    return !attaching||(x->top&&x->initial_sent&&x->state.acknowledged);
}

static inline int mulib_wl_xdg_role_commit(mulib_wl_client *c,void *u,int attaching)
{
    mulib_wl_xdg_resource *x=(mulib_wl_xdg_resource*)u;
    if(!x->top||!x->wl||!mulib_wl_xdg_role_attach(x,attaching))return 0;
    if(!x->initial_sent)return mulib_wl_xdg_send_configure(c,x,0,0,0);
    if(x->wl->attach&&!attaching&&x->wl->surface->current.buffer)
        mulib_wl_xdg_reset_mapping(x);
    return 1;
}
static inline int mulib_wl_xdg_top_request(mulib_wl_client *c,mulib_wl_resource *r,uint16_t op,const unsigned char *p,size_t n){mulib_wl_xdg_resource *x=(mulib_wl_xdg_resource*)r->user;if(!op&&!n)return mulib_wl_resource_remove(&c->objects,r->id)&&mulib_wl_delete_id(c,r->id);if(!x->wl)return 0;if((op==2||op==3)){const char *v;size_t used;if(!mulib_wl_string_read(p,n,&v,&used)||used!=n||strlen(v)>4096)return 0;char *s=mulib_wm_strdup(v);if(!s)return 0;char **d=op==2?&x->title:&x->app_id;free(*d);*d=s;return 1;}if((op==7||op==8)&&n==8){int32_t w=(int32_t)mulib_wl_u32(p),h=(int32_t)mulib_wl_u32(p+4);if(w<0||h<0)return 0;if(op==7){x->max_w=w;x->max_h=h;}else{x->min_w=w;x->min_h=h;}return 1;}if((op==9||op==10||op==12||op==13)&&!n){if(op==9)x->requested|=1u;if(op==10)x->requested&=~1u;if(op==12)x->requested&=~2u;if(op==13)x->requested|=4u;return 1;}if(op==11&&n==4){uint32_t id=mulib_wl_u32(p);mulib_wl_resource *o=id?mulib_wl_resource_find(&c->objects,id):NULL;if(id&&(!o||strcmp(o->interface,"wl_output")))return 0;x->requested|=2u;return 1;}if(op==1&&n==4){/* Parent metadata is not yet used; accept null only. */return mulib_wl_u32(p)==0;}return 0;}
static inline int mulib_wl_xdg_surface_request(mulib_wl_client *c,
        mulib_wl_resource *r,uint16_t op,const unsigned char *p,size_t n)
{
    mulib_wl_xdg_resource *x=(mulib_wl_xdg_resource*)r->user;
    if(!op&&!n&&!x->top)
        return mulib_wl_resource_remove(&c->objects,r->id)&&mulib_wl_delete_id(c,r->id);
    if(!x->wl)return 0;
    if(op==1&&n==4) {
        uint32_t id=mulib_wl_u32(p);
        if(x->role_created||x->top||!mulib_wl_shm_id(c,id)||x->refs==UINT_MAX||
           !mulib_wl_xdg_init(&x->state,x->wl->surface,0,64))return 0;
        if(!mulib_wl_resource_add(&c->objects,(mulib_wl_resource){
            id,1,"xdg_toplevel",mulib_wl_xdg_top_request,x,mulib_wl_xdg_top_destroy}))return 0;
        x->top=id;
        x->role_created=1;
        x->refs++;
        return 1;
    }
    if(!x->top)return 0;
    if(op==3&&n==16) {
        mulib_wm_rect a={(int32_t)mulib_wl_u32(p),(int32_t)mulib_wl_u32(p+4),
                        (int32_t)mulib_wl_u32(p+8),(int32_t)mulib_wl_u32(p+12)};
        if(!mulib_wm_rect_valid(a)||!a.w||!a.h)return 0;
        x->geometry=a;
        return 1;
    }
    if(op==4&&n==4)return mulib_wl_xdg_ack(&x->state,mulib_wl_u32(p),NULL);
    return 0;
}
static inline int mulib_wl_xdg_base_request(mulib_wl_client *c,
        mulib_wl_resource *r,uint16_t op,const unsigned char *p,size_t n)
{
    mulib_wl_xdg_base_resource *b=(mulib_wl_xdg_base_resource*)r->user;
    if(!op&&!n&&!b->live)
        return mulib_wl_resource_remove(&c->objects,r->id)&&mulib_wl_delete_id(c,r->id);
    if(op==3&&n==4)return 1; /* No ping machinery in this subset. */
    if(op!=2||n!=8||b->refs==UINT_MAX||b->live==UINT_MAX)return 0;
    uint32_t id=mulib_wl_u32(p),wid=mulib_wl_u32(p+4);
    mulib_wl_resource *wr=mulib_wl_resource_find(&c->objects,wid);
    if(!mulib_wl_shm_id(c,id)||!wr||wr->request!=mulib_wl_surface_resource_request)return 0;
    mulib_wl_surface_resource *w=(mulib_wl_surface_resource*)wr->user;
    if(w->role_user||w->surface->role||w->surface->current.buffer||w->attach)return 0;
    mulib_wl_xdg_resource *x=(mulib_wl_xdg_resource*)calloc(1,sizeof *x);
    if(!x)return 0;
    x->base=b;
    x->wl=w;
    x->refs=1;
    x->id=id;
    b->refs++;
    b->live++;
    if(!mulib_wl_resource_add(&c->objects,(mulib_wl_resource){
        id,1,"xdg_surface",mulib_wl_xdg_surface_request,x,mulib_wl_xdg_resource_unref})) {
        mulib_wl_xdg_resource_unref(x);
        return 0;
    }
    w->role_user=x;
    w->role_commit=mulib_wl_xdg_role_commit;
    w->role_detach=mulib_wl_xdg_detach;
    w->role_attach=mulib_wl_xdg_role_attach;
    return 1;
}
static inline int mulib_wl_xdg_bind(mulib_wl_client *c,uint32_t id,uint32_t version,void *u){if(!u||version!=1)return 0;mulib_wl_xdg_base_resource *b=(mulib_wl_xdg_base_resource*)calloc(1,sizeof *b);if(!b)return 0;b->owner=(mulib_wl_xdg_server*)u;b->refs=1;if(!mulib_wl_resource_add(&c->objects,(mulib_wl_resource){id,1,"xdg_wm_base",mulib_wl_xdg_base_request,b,mulib_wl_xdg_base_unref})){free(b);return 0;}return 1;}
static inline int mulib_wl_xdg_close(mulib_wl_client *c,const mulib_wl_xdg_resource *x){return x&&x->top&&mulib_wl_client_event(c,x->top,1,NULL,0);}
#endif
