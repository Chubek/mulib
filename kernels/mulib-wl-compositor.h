/* MuLIB notebook desktop extension. C11, header-only.
 * SPDX-License-Identifier: MIT
 * See AGENTS.md for integration and coverage limits. */
#ifndef MULIB_WL_COMPOSITOR_H
#define MULIB_WL_COMPOSITOR_H
#include "mulib-wl-surface.h"
/* Heap allocated surfaces keep stable addresses; ID namespace is compositor-local.
 * Set snapshot_limit to bound bytes in each wire SHM snapshot (0: default). */
typedef struct {
    mulib_wl_surface **v;
    size_t n,cap,limit;
    uint64_t next;
    size_t snapshot_limit;
} mulib_wl_compositor;
static inline mulib_wl_surface *mulib_wl_compositor_create(mulib_wl_compositor *c){if(!c||c->n>=c->limit||c->next==UINT64_MAX||c->n==SIZE_MAX)return NULL;mulib_wl_surface *s=(mulib_wl_surface*)malloc(sizeof *s);if(!s||!mulib_wm_grow((void**)&c->v,&c->cap,c->n+1,sizeof *c->v)){free(s);return NULL;}mulib_wl_surface_init(s,++c->next);c->v[c->n++]=s;return s;}
static inline int mulib_wl_compositor_destroy(mulib_wl_compositor *c,uint64_t id){if(c)for(size_t i=0;i<c->n;i++)if(c->v[i]->id==id){mulib_wl_surface_free(c->v[i]);free(c->v[i]);memmove(c->v+i,c->v+i+1,(--c->n-i)*sizeof *c->v);return 1;}return 0;}
static inline void mulib_wl_compositor_free(mulib_wl_compositor *c){if(c){while(c->n)mulib_wl_compositor_destroy(c,c->v[c->n-1]->id);free(c->v);memset(c,0,sizeof *c);}}
#include "mulib-wl-shm.h"
#include "mulib-wl-frame.h"
#include "mulib-glx-occlusion.h"

typedef struct {mulib_wm_rect *v;size_t n,cap;} mulib_wl_region_resource;
/* Minimal wl_compositor v1 wire front end. Shell roles are attached by host.
 * The current CPU buffer is a commit-time snapshot, not borrowed client memory. */
typedef struct {
    mulib_wl_compositor *owner;
    mulib_wl_surface *surface;
    mulib_wl_shm_resource *pending;
    int attach;
    int32_t x,y;
    mulib_wl_frames pending_frames,committed_frames;
    mulib_wl_region_resource input,pending_input;
    int input_default,pending_input_default,input_changed;
    void *role_user;
    int (*role_commit)(mulib_wl_client *,void *,int);
    void (*role_detach)(void *);
    int (*role_attach)(void *,int);
    mulib_wl_account *account;
} mulib_wl_surface_resource;
static inline void mulib_wl_surface_resource_free(void *u)
{
    mulib_wl_surface_resource *s=(mulib_wl_surface_resource*)u;
    if(!s)return;
    if(s->role_detach)s->role_detach(s->role_user);
    mulib_wl_shm_resource_unref(s->pending);
    free(s->input.v);
    free(s->pending_input.v);
    mulib_wl_frames_free(&s->pending_frames);
    mulib_wl_frames_free(&s->committed_frames);
    mulib_wl_compositor_destroy(s->owner,s->surface->id);
    mulib_wl_account_release(s->account,(mulib_wl_usage){.surfaces=1});
    free(s);
}
/* Accounting follows the actual CPU image, which may outlive a surface
 * resource or the client if the host retained an image reference. */
typedef struct {
    mulib_wl_buffer b;
    void *pixels;
    mulib_wl_account *account;
    size_t bytes;
} mulib_wl_owned_snapshot;
static inline void mulib_wl_owned_snapshot_free(void *u)
{
    mulib_wl_owned_snapshot *s=(mulib_wl_owned_snapshot*)u;
    free(s->pixels);
    mulib_wl_account_release(s->account,(mulib_wl_usage){.snapshot_bytes=s->bytes});
    free(s);
}
static inline int mulib_wl_callback_request(mulib_wl_client *c,mulib_wl_resource *r,uint16_t op,const unsigned char *p,size_t n){(void)c;(void)r;(void)op;(void)p;(void)n;return 0;/* wl_callback has no requests by specification */}
static inline int mulib_wl_surface_resource_commit(mulib_wl_client *c,
                                                  mulib_wl_surface_resource *s)
{
    if(s->pending_frames.n>s->committed_frames.limit-s->committed_frames.n||
       !mulib_wm_grow((void**)&s->committed_frames.v,&s->committed_frames.cap,
                      s->committed_frames.n+s->pending_frames.n,sizeof(uint32_t)))return 0;
    if(s->role_commit&&!s->role_commit(c,s->role_user,s->attach&&s->pending))return 0;
    if(s->attach) {
        mulib_wl_owned_snapshot *snap=NULL;
        if(s->pending) {
            mulib_wl_shm_resource *b=s->pending;
            size_t limit=s->owner->snapshot_limit;
            if(!limit||limit>64u*1024u*1024u)limit=64u*1024u*1024u;
            if((size_t)b->w>limit/sizeof(uint32_t)/(size_t)b->h)return 0;
            size_t bytes=(size_t)b->w*(size_t)b->h*sizeof(uint32_t);
            snap=(mulib_wl_owned_snapshot*)calloc(1,sizeof *snap);
            if(!snap)return 0;
            if(!mulib_wl_account_charge(s->account,(mulib_wl_usage){.snapshot_bytes=bytes})) {
                free(snap);
                return 0;
            }
            snap->bytes=bytes;
            snap->account=s->account;
            if(!mulib_wl_shm_snapshot(&snap->b,b->pool->fd,b->pool->size,
                                      b->offset,b->w,b->h,b->stride,b->xrgb)) {
                mulib_wl_owned_snapshot_free(snap);
                return 0;
            }
            snap->pixels=(void*)snap->b.image.pixels;
            snap->b.image.user=snap;
            snap->b.image.release=mulib_wl_owned_snapshot_free;
        }
        int ok=mulib_wl_surface_attach(s->surface,snap?&snap->b:NULL,s->x,s->y);
        if(snap)mulib_wl_buffer_destroy(&snap->b);
        if(!ok)return 0;
    }
    if(!mulib_wl_surface_commit(s->surface))return 0;
    if(s->attach) {
        /* The renderer owns a separate copy: release is not a frame event. */
        if(s->pending&&s->pending->alive&&
           !mulib_wl_client_event(c,s->pending->id,0,NULL,0))return 0;
        mulib_wl_shm_resource_unref(s->pending);
        s->pending=NULL;
        s->attach=0;
    }
    if(s->input_changed) {
        free(s->input.v);
        s->input=s->pending_input;
        s->pending_input=(mulib_wl_region_resource){0};
        s->input_default=s->pending_input_default;
        s->input_changed=0;
    }
    if(s->pending_frames.n) {
        memcpy(s->committed_frames.v+s->committed_frames.n,s->pending_frames.v,
               s->pending_frames.n*sizeof(uint32_t));
        s->committed_frames.n+=s->pending_frames.n;
        s->pending_frames.n=0;
    }
    return 1;
}
static inline int mulib_wl_surface_resource_request(mulib_wl_client *c,
        mulib_wl_resource *r,uint16_t op,const unsigned char *p,size_t n)
{
    mulib_wl_surface_resource *s=(mulib_wl_surface_resource*)r->user;
    if(op==0&&n==0) {
        /* Destroy the protocol role object before its wl_surface. Disconnect
         * cleanup instead calls resource destructors directly, in any order. */
        if(s->role_user)return 0;
        mulib_wl_frames *lists[]={&s->pending_frames,&s->committed_frames};
        for(size_t k=0;k<2;k++)for(size_t i=0;i<lists[k]->n;i++) {
            uint32_t id=lists[k]->v[i];
            if(!mulib_wl_resource_remove(&c->objects,id)||!mulib_wl_delete_id(c,id))return 0;
        }
        return mulib_wl_resource_remove(&c->objects,r->id)&&mulib_wl_delete_id(c,r->id);
    }
    if(op==1&&n==12) {
        uint32_t id=mulib_wl_u32(p);
        if(s->role_attach&&!s->role_attach(s->role_user,id!=0))return 0;
        mulib_wl_shm_resource *b=NULL;
        if(id) {
            mulib_wl_resource *br=mulib_wl_resource_find(&c->objects,id);
            if(!br||br->request!=mulib_wl_shm_buffer_request)return 0;
            b=(mulib_wl_shm_resource*)br->user;
            if(b->refs==UINT_MAX)return 0;
            b->refs++;
        }
        mulib_wl_shm_resource_unref(s->pending);
        s->pending=b;
        s->attach=1;
        s->x=(int32_t)mulib_wl_u32(p+4);
        s->y=(int32_t)mulib_wl_u32(p+8);
        return 1;
    }
    if(op==2&&n==16) {
        s->surface->damage.full=1;
        return 1;
    }
    if(op==3&&n==4) {
        uint32_t id=mulib_wl_u32(p);
        if(!mulib_wl_shm_id(c,id)||!mulib_wl_frame_add(&s->pending_frames,id))return 0;
        if(!mulib_wl_resource_add(&c->objects,(mulib_wl_resource){
            id,1,"wl_callback",mulib_wl_callback_request,NULL,NULL})) {
            s->pending_frames.n--;
            return 0;
        }
        return 1;
    }
    if((op==4||op==5)&&n==4) {
        uint32_t id=mulib_wl_u32(p);
        mulib_wl_resource *a=id?mulib_wl_resource_find(&c->objects,id):NULL;
        if(id&&(!a||strcmp(a->interface,"wl_region")))return 0;
        if(op==4)return 1; /* Opaque hint: never used for culling here. */
        mulib_wl_region_resource next={0};
        if(a) {
            mulib_wl_region_resource *src=(mulib_wl_region_resource*)a->user;
            if(src->n&&!mulib_wm_grow((void**)&next.v,&next.cap,src->n,sizeof *next.v))return 0;
            next.n=src->n;
            if(next.n)memcpy(next.v,src->v,next.n*sizeof *next.v);
        }
        free(s->pending_input.v);
        s->pending_input=next;
        s->pending_input_default=!id;
        s->input_changed=1;
        return 1;
    }
    if(op==6&&n==0)return mulib_wl_surface_resource_commit(c,s);
    return 0;
}
/* Region storage via rectangle collection with subtraction. */
static inline void mulib_wl_region_free(void *u){mulib_wl_region_resource *r=(mulib_wl_region_resource*)u;free(r->v);free(r);}
static inline int mulib_wl_region_request(mulib_wl_client *c,mulib_wl_resource *r,uint16_t op,const unsigned char *p,size_t n){if(!op&&!n)return mulib_wl_resource_remove(&c->objects,r->id)&&mulib_wl_delete_id(c,r->id);if((op!=1&&op!=2)||n!=16)return 0;mulib_wm_rect a={(int32_t)mulib_wl_u32(p),(int32_t)mulib_wl_u32(p+4),(int32_t)mulib_wl_u32(p+8),(int32_t)mulib_wl_u32(p+12)};if(!mulib_wm_rect_valid(a))return 0;if(!a.w||!a.h)return 1;mulib_wl_region_resource *q=(mulib_wl_region_resource*)r->user;if(op==1){if(q->n>=4096||!mulib_wm_grow((void**)&q->v,&q->cap,q->n+1,sizeof a))return 0;q->v[q->n++]=a;return 1;}mulib_wl_region_resource t={0};for(size_t i=0;i<q->n;i++){mulib_wm_rect out[4];size_t k=mulib_glx_occlusion_subtract(q->v[i],a,out);if(t.n+k>4096||!mulib_wm_grow((void**)&t.v,&t.cap,t.n+k,sizeof a)){free(t.v);return 0;}if(k)memcpy(t.v+t.n,out,k*sizeof a);t.n+=k;}free(q->v);*q=t;return 1;}
static inline int mulib_wl_compositor_request(mulib_wl_client *c,mulib_wl_resource *r,
        uint16_t op,const unsigned char *p,size_t n)
{
    if(n!=4||op>1)return 0;
    uint32_t id=mulib_wl_u32(p);
    if(!mulib_wl_shm_id(c,id))return 0;
    if(op==1) {
        mulib_wl_region_resource *q=(mulib_wl_region_resource*)calloc(1,sizeof *q);
        if(!q)return 0;
        if(!mulib_wl_resource_add(&c->objects,(mulib_wl_resource){
            id,1,"wl_region",mulib_wl_region_request,q,mulib_wl_region_free})) {
            free(q);
            return 0;
        }
        return 1;
    }
    mulib_wl_surface_resource *s=(mulib_wl_surface_resource*)calloc(1,sizeof *s);
    if(!s)return 0;
    if(!mulib_wl_account_charge(c->account,(mulib_wl_usage){.surfaces=1})) {
        free(s);
        return 0;
    }
    s->account=c->account;
    s->owner=(mulib_wl_compositor*)r->user;
    s->surface=mulib_wl_compositor_create(s->owner);
    if(!s->surface) {
        mulib_wl_account_release(s->account,(mulib_wl_usage){.surfaces=1});
        free(s);
        return 0;
    }
    s->pending_frames.limit=s->committed_frames.limit=256;
    s->input_default=s->pending_input_default=1;
    if(!mulib_wl_resource_add(&c->objects,(mulib_wl_resource){
        id,1,"wl_surface",mulib_wl_surface_resource_request,s,mulib_wl_surface_resource_free})) {
        mulib_wl_surface_resource_free(s);
        return 0;
    }
    return 1;
}
static inline int mulib_wl_compositor_bind(mulib_wl_client *c,uint32_t id,uint32_t version,void *u){return u&&version==1&&mulib_wl_resource_add(&c->objects,(mulib_wl_resource){id,version,"wl_compositor",mulib_wl_compositor_request,u,NULL});}

/* Test local surface coordinates against the committed input region. */
static inline int mulib_wl_surface_resource_accepts(const mulib_wl_surface_resource *s,double x,double y){if(!s||!isfinite(x)||!isfinite(y)||!s->surface->current.buffer)return 0;const mulib_wl_buffer *b=s->surface->current.buffer;if(x<0||y<0||x>=b->image.w||y>=b->image.h)return 0;if(s->input_default)return 1;for(size_t i=0;i<s->input.n;i++)if(mulib_wm_contains(s->input.v[i],x,y))return 1;return 0;}
#endif
