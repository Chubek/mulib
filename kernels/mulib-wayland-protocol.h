/* MuLIB graphical extension: wayland-protocol. C11, header-only. */
#ifndef MULIB_WAYLAND_PROTOCOL_H
#define MULIB_WAYLAND_PROTOCOL_H
#include "mulib-wayland-wire.h"
/* Shared request/event shape validator. Interfaces are in object/new-id argument
 * order. check() enforces the caller's (client or server) object namespace.
 * FDs have no wire bytes and remain an ordered transport queue. */
typedef int (*mway_object_check_fn)(void *,uint32_t,const char *,int);
typedef struct {
    uint32_t since;
    const char *signature;
    const char *const *interfaces;
} mway_signature;
static inline int mulib_wayland_protocol_validate(const mway_signature *desc,
        uint32_t version,const void *data,size_t n,mway_object_check_fn check,
        void *user,size_t *fd_count) {
    if(!desc||!desc->since||version<desc->since||!desc->signature||
       (!data&&n)||n%4||!fd_count)return 0;
    const unsigned char *p=data;
    size_t at=0,objects=0,fds=0;
    for(const char *s=desc->signature;*s;s++) {
        int nullable=0;
        if(*s=='?') { nullable=1; s++; if(!*s)return 0; }
        if(nullable&&*s!='o'&&*s!='s')return 0;
        if(*s=='h') { fds++; continue; }
        if(at>n||n-at<4)return 0;
        uint32_t v;
        memcpy(&v,p+at,4);
        at+=4;
        if(*s=='s'||*s=='a') {
            if(*s=='s'&&!v&&!nullable)return 0;
            if((size_t)v>n-at)return 0;
            size_t padded=((size_t)v+3)&~(size_t)3;
            if(padded>n-at)return 0;
            if(*s=='s'&&v&&(p[at+v-1]||memchr(p+at,0,v-1)))return 0;
            at+=padded;
        } else if(*s=='o'||*s=='n') {
            const char *iface=desc->interfaces?desc->interfaces[objects]:NULL;
            objects++;
            if(!v) { if(!nullable)return 0; }
            else if(!check||!check(user,v,iface,*s=='n'))return 0;
        } else if(!strchr("iuf",*s))return 0;
    }
    if(at!=n)return 0;
    *fd_count=fds;
    return 1;
}
typedef void (*mway_event_fn)(void *,const mway_frame *);
typedef struct{uint32_t id,version;const char *interface;mway_event_fn event;void *user;}mway_object;
typedef struct{int fd;uint32_t next;mway_object *v;size_t n,cap;}mulib_wayland_protocol_t;
static inline uint32_t mulib_wayland_protocol_new(mulib_wayland_protocol_t *p,const char *iface,uint32_t ver,mway_event_fn fn,void *u){if(!p||!iface||!*iface||!ver||p->n==SIZE_MAX){errno=EINVAL;return 0;}if(p->next<1)p->next=1;if(p->next>=0xfeffffff||!mgx_grow((void**)&p->v,&p->cap,p->n+1,sizeof *p->v))return 0;uint32_t id=++p->next;p->v[p->n++]=(mway_object){id,ver,iface,fn,u};return id;}
static inline int mulib_wayland_protocol_dispatch(mulib_wayland_protocol_t *p,const mway_frame *f){if(!p||!f||!f->object||(!f->args&&f->n)||f->n%4)return 0;for(size_t i=0;i<p->n;i++)if(p->v[i].id==f->object){mway_object o=p->v[i];if(o.event)o.event(o.user,f);return 1;}return f->object==1;}
static inline int mulib_wayland_protocol_bind(int fd,uint32_t registry,uint32_t global,const char *iface,uint32_t version,uint32_t object){if(!iface||!*iface||!version||!object){errno=EINVAL;return 0;}mway_bytes b={0};int ok=mway_u32(&b,global)&&mway_string(&b,iface)&&mway_u32(&b,version)&&mway_u32(&b,object)&&mulib_wayland_wire_send(fd,registry,0,b.v,b.n,-1)==0;mway_bytes_free(&b);return ok;}
static inline int mulib_wayland_protocol_registry(int fd,uint32_t id){if(!id){errno=EINVAL;return 0;}return mulib_wayland_wire_send(fd,1,1,&id,4,-1)==0;}
static inline void mulib_wayland_protocol_free(mulib_wayland_protocol_t *p){if(!p)return;free(p->v);p->v=NULL;p->n=p->cap=0;}
#endif
