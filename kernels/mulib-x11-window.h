/* MuLIB graphical extension: x11-window. C11, header-only. */
#ifndef MULIB_X11_WINDOW_H
#define MULIB_X11_WINDOW_H
#include "mulib-x11-wire.h"
static inline int mulib_x11_window_create(mx11_connection *c,uint32_t id,uint32_t parent,int16_t x,int16_t y,uint16_t w,uint16_t h,uint32_t eventmask){if(!id||!parent||!w||!h)return 0;unsigned char p[32]={0};mx11_put32(p,id);mx11_put32(p+4,parent);mx11_put16(p+8,(uint16_t)x);mx11_put16(p+10,(uint16_t)y);mx11_put16(p+12,w);mx11_put16(p+14,h);mx11_put16(p+18,1);mx11_put32(p+24,1u<<11);mx11_put32(p+28,eventmask);return mulib_x11_wire_request(c,1,0,p,sizeof p);}
static inline int mulib_x11_window_map(mx11_connection *c,uint32_t w){if(!w){errno=EINVAL;return 0;}unsigned char p[4];mx11_put32(p,w);return mulib_x11_wire_request(c,8,0,p,4);}
static inline int mulib_x11_window_destroy(mx11_connection *c,uint32_t w){if(!w){errno=EINVAL;return 0;}unsigned char p[4];mx11_put32(p,w);return mulib_x11_wire_request(c,4,0,p,4);}
static inline int mulib_x11_window_gc(mx11_connection *c,uint32_t id,uint32_t drawable){if(!id||!drawable){errno=EINVAL;return 0;}unsigned char p[12]={0};mx11_put32(p,id);mx11_put32(p+4,drawable);return mulib_x11_wire_request(c,55,0,p,12);}

typedef struct {
    uint32_t root,parent;
    size_t children;
    unsigned char *reply;
} mulib_x11_window_tree;
/* Reply helpers: 1 accepted, 0 X11 rejection, -1 local/transport/bad reply.
 * QueryTree output must be empty; free it once before reusing. */
static inline int mulib_x11_window_query_tree(mx11_stream *s,uint32_t w,
        size_t child_limit,mulib_x11_window_tree *out) {
    if(!w||!out||out->reply) { errno=EINVAL; return -1; }
    unsigned char body[4],*reply;
    size_t n;
    mx11_put32(body,w);
    int r=mulib_x11_wire_reply(s,15,0,body,sizeof body,&reply,&n);
    if(r!=1)return r;
    size_t children=mx11_get16(reply+16);
    uint32_t root=mx11_get32(reply+8),parent=mx11_get32(reply+12);
    if(!root||n!=32+children*4||(!parent&&root!=w)) {
        free(reply);
        return mx11_stream_fail(s,EPROTO);
    }
    if(children>child_limit) { free(reply); errno=E2BIG; return -1; }
    *out=(mulib_x11_window_tree){root,parent,children,reply};
    return 1;
}
static inline uint32_t mulib_x11_window_tree_child(const mulib_x11_window_tree *t,size_t i) {
    return t&&t->reply&&i<t->children?mx11_get32(t->reply+32+4*i):0;
}
static inline void mulib_x11_window_tree_free(mulib_x11_window_tree *t) {
    if(t) { free(t->reply); *t=(mulib_x11_window_tree){0}; }
}
typedef struct { uint16_t class_id; uint8_t map_state,override_redirect; } mulib_x11_window_attrs;
static inline int mulib_x11_window_attributes(mx11_stream *s,uint32_t w,mulib_x11_window_attrs *out) {
    if(!w||!out) { errno=EINVAL; return -1; }
    unsigned char body[4],*reply;
    size_t n;
    mx11_put32(body,w);
    int r=mulib_x11_wire_reply(s,3,0,body,sizeof body,&reply,&n);
    if(r!=1)return r;
    uint16_t cls=mx11_get16(reply+12);
    uint8_t map=reply[26],override=reply[27];
    free(reply);
    if(n!=44||(cls!=1&&cls!=2)||map>2||override>1)return mx11_stream_fail(s,EPROTO);
    *out=(mulib_x11_window_attrs){cls,map,override};
    return 1;
}
typedef struct {
    uint32_t root;
    int16_t x,y;
    uint16_t width,height,border;
} mulib_x11_window_geometry;
static inline int mulib_x11_window_get_geometry(mx11_stream *s,uint32_t w,mulib_x11_window_geometry *out) {
    if(!w||!out) { errno=EINVAL; return -1; }
    unsigned char body[4],*reply;
    size_t n;
    mx11_put32(body,w);
    int r=mulib_x11_wire_reply(s,14,0,body,sizeof body,&reply,&n);
    if(r!=1)return r;
    mulib_x11_window_geometry g={mx11_get32(reply+8),(int16_t)mx11_get16(reply+12),
        (int16_t)mx11_get16(reply+14),mx11_get16(reply+16),mx11_get16(reply+18),mx11_get16(reply+20)};
    free(reply);
    if(n!=32||!g.root||!g.width||!g.height)return mx11_stream_fail(s,EPROTO);
    *out=g;
    return 1;
}
#endif
