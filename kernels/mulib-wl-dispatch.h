/* MuLIB notebook desktop extension. C11, header-only.
 * SPDX-License-Identifier: MIT */
#ifndef MULIB_WL_DISPATCH_H
#define MULIB_WL_DISPATCH_H
#include "mulib-wl-client.h"
static inline uint32_t mulib_wl_u32(const unsigned char *p) {
    uint32_t n; memcpy(&n,p,4); return n;
}
static inline int mulib_wl_check_object(void *u, uint32_t id,
        const char *iface, int is_new) {
    mulib_wl_client *c = u;
    mulib_wl_resource *r = mulib_wl_resource_find(&c->objects,id);
    if (is_new) return id >= 2 && id < 0xff000000u && !r;
    return r && (!iface || !strcmp(r->interface,iface));
}
/* Closed request tables for the built-in wire subset. NULL for unknown
 * interfaces: custom callbacks must validate their own arguments/FDs. */
static inline int mulib_wl_request_shape(const char *iface, uint16_t op,
        mway_signature *out) {
    const char *sig = NULL;
    static const char *const buffer[] = {"wl_buffer"};
    static const char *const region[] = {"wl_region"};
    static const char *const surface[] = {NULL,"wl_surface"};
    static const char *const parent[] = {"xdg_toplevel"};
    static const char *const output[] = {"wl_output"};
    *out = (mway_signature){1,NULL,NULL};
    if (!strcmp(iface,"wl_display")) {
        if (op < 2) sig="n";
    } else if (!strcmp(iface,"wl_registry")) {
        if (!op) sig="usun";
    } else if (!strcmp(iface,"wl_compositor")) {
        if (op < 2) sig="n";
    } else if (!strcmp(iface,"wl_surface")) {
        static const char *const v[] = {"","?oii","iiii","n","?o","?o",""};
        if (op < 7) sig=v[op];
        if (op==1) out->interfaces=buffer;
        if (op==4 || op==5) out->interfaces=region;
    } else if (!strcmp(iface,"wl_region")) {
        if (op < 3) sig=op?"iiii":"";
    } else if (!strcmp(iface,"wl_shm")) {
        if (!op) sig="nhi";
    } else if (!strcmp(iface,"wl_shm_pool")) {
        if (op < 3) sig=op==0?"niiiiu":op==1?"":"i";
    } else if (!strcmp(iface,"wl_buffer")) {
        if (!op) sig="";
    } else if (!strcmp(iface,"wl_callback")) {
        return 0; /* No requests. */
    } else if (!strcmp(iface,"xdg_wm_base")) {
        if (op==0) sig="";
        if (op==2) { sig="no"; out->interfaces=surface; }
        if (op==3) sig="u";
    } else if (!strcmp(iface,"xdg_surface")) {
        if (op==0) sig="";
        if (op==1) sig="n";
        if (op==3) sig="iiii";
        if (op==4) sig="u";
    } else if (!strcmp(iface,"xdg_toplevel")) {
        if (op==0 || op==9 || op==10 || op==12 || op==13) sig="";
        if (op==1) { sig="?o"; out->interfaces=parent; }
        if (op==2 || op==3) sig="s";
        if (op==7 || op==8) sig="ii";
        if (op==11) { sig="?o"; out->interfaces=output; }
    } else return -1;
    out->signature=sig;
    return sig!=NULL;
}
/* Returns 2 if another complete request remains after the budget, 1 drained or
 * waiting for bytes/FDs, -1 fatal/EOF. Reschedule 2 without waiting for POLLIN.
 * Callbacks may remove their own resource, not re-enter dispatch or read RX. */
static inline int mulib_wl_dispatch(mulib_wl_client *c, size_t budget) {
    if (!c || c->failed || c->dispatching) return -1;
    while (budget && c->rn>=8) {
        mway_frame frame;
        int parsed=mulib_wayland_wire_parse(c->rx,c->rn,&frame);
        if (parsed<0) return mulib_wl_client_fail(c);
        if (!parsed) break;
        mulib_wl_resource *r=mulib_wl_resource_find(&c->objects,frame.object);
        if (!r) return mulib_wl_client_fail(c);
        mulib_wl_resource snapshot=*r;
        mway_signature shape;
        size_t fds=0;
        int typed=mulib_wl_request_shape(r->interface,frame.opcode,&shape);
        if (!typed || (typed>0 &&
            !mulib_wayland_protocol_validate(&shape,r->version,frame.args,
                frame.n,mulib_wl_check_object,c,&fds)))
            return mulib_wl_client_fail(c);
        if (fds>c->nfds) {
            if (c->eof) return mulib_wl_client_fail(c);
            return 1;
        }
        c->dispatching=1; c->request_fds=typed>0?fds:c->nfds;
        int ok=snapshot.request(c,&snapshot,frame.opcode,frame.args,frame.n);
        if (typed>0 && c->request_fds) ok=0;
        c->dispatching=0; c->request_fds=0;
        if (!ok || c->failed) return mulib_wl_client_fail(c);
        memmove(c->rx,c->rx+frame.consumed,c->rn-frame.consumed);
        c->rn-=frame.consumed;
        budget--;
    }
    if (c->rn>=8) {
        mway_frame f;
        int r=mulib_wayland_wire_parse(c->rx,c->rn,&f);
        if (r<0) return mulib_wl_client_fail(c);
        if (r>0) return 2;
    }
    if (c->eof) return mulib_wl_client_fail(c);
    return 1;
}
static inline int mulib_wl_string_read(const unsigned char *p,size_t n,
        const char **out,size_t *used) {
    if (!p || n<4 || !out || !used) return 0;
    size_t z=mulib_wl_u32(p);
    if (!z || z>n-4 || z>65520) return 0;
    size_t pad=(z+3)&~(size_t)3;
    if (pad>n-4 || p[4+z-1] || memchr(p+4,0,z-1)) return 0;
    *out=(const char *)p+4; *used=4+pad; return 1;
}
static inline int mulib_wl_delete_id(mulib_wl_client *c,uint32_t id) {
    return id>1 && id<0xff000000u && mulib_wl_client_event(c,1,1,&id,4);
}
#endif
