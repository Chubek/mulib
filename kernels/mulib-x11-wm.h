/* MuLIB graphical extension: bounded X11 property requests and reference
 * non-reparenting window manager for one private X11 screen. C11. */
#ifndef MULIB_X11_WM_H
#define MULIB_X11_WM_H
#include "mulib-x11-wire.h"
#include "mulib-x11-events.h"
#include "mulib-x11-window.h"
#include "mulib-x11-reparent.h"
#include "mulib-x11-icccm.h"
#include "mulib-wm-focus.h"
/* data contains wire-order elements, not endian-converted host integers. */
static inline int mulib_x11_wm_property(mx11_connection *c,uint32_t w,uint32_t property,
        uint32_t type,uint8_t format,const void *data,size_t n) {
    size_t unit=format==8?1:format==16?2:format==32?4:0;
    if(!c||c->failed||!w||!property||!type||!unit||n%unit||
       n>262000||(!data&&n)) { errno=EINVAL; return 0; }
    size_t padded=(n+3)&~(size_t)3;
    if(6+padded/4>c->max_request) { errno=EMSGSIZE; return 0; }
    unsigned char *p=(unsigned char *)calloc(20+padded,1);
    if(!p)return 0;
    mx11_put32(p,w); mx11_put32(p+4,property); mx11_put32(p+8,type);
    p[12]=format; mx11_put32(p+16,(uint32_t)(n/unit));
    if(n)memcpy(p+20,data,n);
    int ok=mulib_x11_wire_request(c,18,0,p,20+padded);
    free(p);
    return ok;
}
static inline int mulib_x11_wm_title(mx11_connection *c,uint32_t w,const char *title) {
    return title&&mulib_x11_wm_property(c,w,39,31,8,title,strlen(title));
}

/* Owns the registry, borrows the dedicated stream. One owner thread. The
 * registry holds mapped, non-override InputOutput root children only.
 * Geometry is in root coordinates (outer origin, inner dimensions), and
 * focused is the last acknowledged WM focus choice, not an X11 focus monitor.
 * This synchronous slice does not grab the server across queries/actions:
 * destroying, unmapping or reparenting a client can race the WM. */
typedef struct {
    mx11_stream *stream;
    uint32_t root;
    size_t max_clients;
    mulib_wm_core core;
} mulib_x11_wm;

static inline mulib_wm_window *mulib_x11_wm_find_native(mulib_x11_wm *wm,uint32_t w) {
    if(wm&&w)for(size_t i=0;i<wm->core.n;i++)if(wm->core.v[i].native==w)return wm->core.v+i;
    return NULL;
}
static inline int mx11_wm_race(mx11_stream *s,int r,int bad_match) {
    return r==0&&(s->error.code==3||(bad_match&&s->error.code==8)); /* BadWindow/BadMatch */
}
static inline int mx11_wm_window_request(mx11_stream *s,uint8_t op,uint32_t w) {
    unsigned char body[4];
    mx11_put32(body,w);
    return mulib_x11_wire_checked(s,op,0,body,sizeof body);
}
static inline int mx11_wm_set_focus(mulib_x11_wm *wm,uint32_t w) {
    unsigned char body[8]={0}; /* CurrentTime */
    mx11_put32(body,w);
    return mulib_x11_wire_checked(wm->stream,42,1,body,sizeof body); /* RevertToPointerRoot */
}
/* Explicit focus of a tracked client (1), skipped for unavailable clients or
 * InputHint=false (0), failure (-1). Does not implement WM_TAKE_FOCUS. */
static inline int mulib_x11_wm_focus_window(mulib_x11_wm *wm,uint32_t w) {
    mulib_wm_window *entry=mulib_x11_wm_find_native(wm,w);
    if(!mulib_wm_focusable(entry,0))return 0;
    if(wm->core.revision==UINT64_MAX) { errno=EOVERFLOW; return -1; }
    mulib_x11_wm_io io=mulib_x11_wm_wire_io(wm->stream);
    mulib_x11_property hints={0};
    if(!mulib_x11_property_get(&io,w,35,35,0,2,0,&hints)) {
        if(mx11_wm_race(wm->stream,0,0))return 0;
        return -1;
    }
    uint32_t flags=0,input=1;
    (void)mulib_x11_property_u32(&hints,0,&flags);
    (void)mulib_x11_property_u32(&hints,1,&input);
    mulib_x11_property_free(&hints);
    if((flags&1)&&!input)return 0; /* WM_HINTS InputHint */
    int r=mx11_wm_set_focus(wm,w);
    if(r==1) { (void)mulib_wm_focus(&wm->core,entry->id,0); return 1; }
    return mx11_wm_race(wm->stream,r,1)?0:-1;
}
static inline int mx11_wm_fallback_focus(mulib_x11_wm *wm) {
    for(size_t i=wm->core.n;i;i--) {
        int r=mulib_x11_wm_focus_window(wm,(uint32_t)wm->core.v[i-1].native);
        if(r)return r;
    }
    /* Focus PointerRoot, not one screen's actual root window. */
    return mx11_wm_set_focus(wm,1)==1?1:-1;
}
static inline int mx11_wm_direct_child(mulib_x11_wm *wm,uint32_t w) {
    mulib_x11_window_tree tree={0};
    int r=mulib_x11_window_query_tree(wm->stream,w,UINT16_MAX,&tree);
    if(r!=1)return mx11_wm_race(wm->stream,r,0)?0:-1;
    int direct=tree.parent==wm->root&&tree.root==wm->root;
    mulib_x11_window_tree_free(&tree);
    return direct;
}
static inline int mx11_wm_admit(mulib_x11_wm *wm,uint32_t w,int verify_parent) {
    mx11_stream *s=wm->stream;
    if(!w||w==wm->root)return 0;
    if(wm->core.n>=wm->max_clients||mulib_x11_wm_find_native(wm,w))return 0;
    if(verify_parent) {
        int r=mx11_wm_direct_child(wm,w);
        if(r!=1)return r;
    }
    mulib_x11_window_attrs a;
    int r=mulib_x11_window_attributes(s,w,&a);
    if(r!=1)return mx11_wm_race(s,r,0)?0:-1;
    if(a.class_id!=1||a.override_redirect||a.map_state!=2)return 0;
    mulib_x11_window_geometry g;
    r=mulib_x11_window_get_geometry(s,w,&g);
    if(r!=1)return r==0&&s->error.code==9?0:-1; /* BadDrawable: destroyed after attributes. */
    if(g.root!=wm->root)return 0;
    if(wm->core.next==UINT64_MAX||wm->core.revision>=UINT64_MAX-1) {
        errno=EOVERFLOW;
        return -1;
    }
    uint64_t id=mulib_wm_add(&wm->core,MULIB_WM_X11,w,NULL,NULL);
    if(!id) { errno=ENOMEM; return -1; }
    mulib_wm_find(&wm->core,id)->flags=MULIB_WM_MAPPED|MULIB_WM_FLOATING;
    (void)mulib_wm_place(&wm->core,id,(mulib_wm_rect){g.x,g.y,g.width,g.height});
    return 1;
}
/* Dispose of local state only. Closing the dedicated X connection releases
 * redirection. No windows are reparented or owned by this WM. */
static inline void mulib_x11_wm_clear(mulib_x11_wm *wm) {
    if(wm) { mulib_wm_core_free(&wm->core); *wm=(mulib_x11_wm){0}; }
}
/* Empty wm required. 1 running, 0 another WM (BadAccess, EBUSY), -1 failure.
 * Startup scans at most max_startup root children (else fails); a full client
 * registry leaves excess clients mapped/unmanaged. After any startup failure
 * close the dedicated connection; redirection may already have been set. */
static inline int mulib_x11_wm_start(mulib_x11_wm *wm,mx11_stream *s,
        uint32_t root,size_t max_clients,size_t max_startup) {
    if(!wm||wm->stream||!s||!root||!max_clients||!max_startup) { errno=EINVAL; return -1; }
    mulib_x11_wm_io io=mulib_x11_wm_wire_io(s);
    if(!mulib_x11_reparent_select(&io,root,(1u<<20)|(1u<<19))) {
        if(s->error.code==10) { errno=EBUSY; return 0; } /* BadAccess */
        return -1;
    }
    *wm=(mulib_x11_wm){.stream=s,.root=root,.max_clients=max_clients};
    mulib_x11_window_tree tree={0};
    if(mulib_x11_window_query_tree(s,root,max_startup,&tree)!=1)goto fail;
    if(tree.root!=root||tree.parent) { errno=EINVAL; goto fail; }
    for(size_t i=0;i<tree.children&&wm->core.n<max_clients;i++) {
        uint32_t w=mulib_x11_window_tree_child(&tree,i);
        if(mx11_wm_admit(wm,w,0)<0)goto fail;
    }
    mulib_x11_window_tree_free(&tree);
    if(wm->core.n&&mx11_wm_fallback_focus(wm)<0)goto fail;
    return 1;
fail:
    mulib_x11_window_tree_free(&tree);
    mulib_x11_wm_clear(wm);
    return mx11_stream_fail(s,errno);
}
static inline int mx11_wm_configure(mulib_x11_wm *wm,const mulib_x11_wm_event *e) {
    if(!e->mask||e->mask&~127u)return 0;
    int direct=mx11_wm_direct_child(wm,e->window);
    if(direct!=1)return direct;
    uint32_t values[7],fields[7]={(uint32_t)e->x,(uint32_t)e->y,e->width,e->height,
        e->border,e->sibling,e->detail};
    size_t n=0;
    for(unsigned i=0;i<7;i++)if((e->mask>>i)&1)values[n++]=fields[i];
    mulib_x11_wm_io io=mulib_x11_wm_wire_io(wm->stream);
    if(mulib_x11_reparent_configure(&io,e->window,e->mask,values,n))return 1;
    /* A client or sibling may have vanished or moved to another parent. A
     * rejected configure does not modify registry geometry. */
    unsigned code=wm->stream->error.code;
    return code==2||code==3||code==8?0:-1; /* BadValue/BadWindow/BadMatch */
}
/* Dispatch one wire event. 1 processed, 0 ignored/raced, -1 fatal to the
 * caller. Core state is changed by real root events only (never SendEvent).
 * Configure requests are forwarded unmodified; normal ConfigureNotify tracks
 * real changes. No synthetic no-op/configure negotiation, tiling, or grabs. */
static inline int mulib_x11_wm_dispatch(mulib_x11_wm *wm,const unsigned char *p,size_t n) {
    if(!wm||!wm->stream||!p||n<32) { errno=EINVAL; return -1; }
    if(p[0]&128)return 0; /* Untrusted SendEvent payload is irrelevant to this policy. */
    mulib_x11_wm_event e;
    int r=mulib_x11_events_wm(p,n,&e);
    if(r!=1)return r;
    if(e.event!=wm->root||!e.window||e.window==wm->root)return 0;
    mulib_wm_window *entry=mulib_x11_wm_find_native(wm,e.window);
    switch(e.type) {
    case MULIB_X11_MAP_REQUEST:
        r=mx11_wm_direct_child(wm,e.window);
        if(r!=1)return r;
        r=mx11_wm_window_request(wm->stream,8,e.window);
        return mx11_wm_race(wm->stream,r,0)?0:(r==1?1:-1);
    case MULIB_X11_MAP_NOTIFY:
        if(e.override_redirect)return 0;
        r=mx11_wm_admit(wm,e.window,1);
        if(r==1&&mulib_x11_wm_focus_window(wm,e.window)<0)return -1;
        return r;
    case MULIB_X11_CONFIGURE_REQUEST:
        return mx11_wm_configure(wm,&e);
    case MULIB_X11_CONFIGURE_NOTIFY:
        if(entry&&e.width&&e.height) {
            if(mulib_wm_place(&wm->core,entry->id,(mulib_wm_rect){e.x,e.y,e.width,e.height}))return 1;
            errno=EOVERFLOW;
            return -1;
        }
        return 0;
    case MULIB_X11_REPARENT_NOTIFY:
        if(e.parent==wm->root)return 0; /* MapNotify handles newly adopted children. */
        /* fall through */
    case MULIB_X11_UNMAP_NOTIFY:
    case MULIB_X11_DESTROY_NOTIFY:
        if(entry) {
            int lost_focus=entry->id==wm->core.focused;
            if(!mulib_wm_remove(&wm->core,entry->id)) { errno=EOVERFLOW; return -1; }
            if(lost_focus&&mx11_wm_fallback_focus(wm)<0)return -1;
            return 1;
        }
        return 0;
    default:
        return 0;
    }
}
/* Host-initiated cooperative close, never kill/destroy the client. Caller
 * supplies the two connection-local atoms (e.g. from mulib_x11_atoms_get).
 * 1 delivered, 0 unavailable/unsupported, -1 failure. */
static inline int mulib_x11_wm_request_close(mulib_x11_wm *wm,uint32_t w,
        uint32_t protocols,uint32_t delete_window,uint32_t time) {
    if(!protocols||!delete_window) { errno=EINVAL; return -1; }
    if(!mulib_x11_wm_find_native(wm,w))return 0;
    mulib_x11_wm_io io=mulib_x11_wm_wire_io(wm->stream);
    mulib_x11_property p={0};
    if(!mulib_x11_property_get(&io,w,protocols,4,0,256,0,&p))
        return mx11_wm_race(wm->stream,0,0)?0:-1;
    int found=0;
    for(size_t i=0;!p.remaining&&i<p.items;i++) {
        uint32_t atom;
        if(mulib_x11_property_u32(&p,i,&atom)&&atom==delete_window) { found=1; break; }
    }
    mulib_x11_property_free(&p);
    if(!found)return 0;
    if(mulib_x11_icccm_protocol(&io,w,protocols,delete_window,time))return 1;
    return mx11_wm_race(wm->stream,0,0)?0:-1;
}
#endif
