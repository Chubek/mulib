/* MuLIB graphical extension: bounded core X11 event decoding. C11. */
#ifndef MULIB_X11_EVENTS_H
#define MULIB_X11_EVENTS_H
#include "mulib-x11-wire.h"
#include "mulib-glx-event.h"
/* Native X11 keycodes, not keysyms or evdev codes. XKB mapping is external.
 * 1 supported event, 0 unsupported, -1 malformed/truncated input.
 * Output is unchanged unless a supported event is decoded. */
static inline int mulib_x11_events_decode_bounded(const unsigned char *p,size_t n,mgx_event *e) {
    if(!p||!e||n<32) { errno=EINVAL; return -1; }
    unsigned type=p[0]&127;
    mgx_event out={0};
    if(type>=2&&type<=6) {
        out.time_us=(uint64_t)mx11_get32(p+4)*1000;
        out.window=mx11_get32(p+12);
        out.x=(int16_t)mx11_get16(p+24); out.y=(int16_t)mx11_get16(p+26);
        out.mods=mx11_get16(p+28); out.code=p[1];
        if(type<=3) { out.type=MGX_EVENT_KEY; out.down=type==2; }
        else if(type<=5) {
            if(!p[1]) { errno=EPROTO; return -1; }
            if(p[1]>=4&&p[1]<=7) {
                if(type==5)return 0;
                out.type=MGX_EVENT_SCROLL;
                out.dy=p[1]==4?1:p[1]==5?-1:0;
                out.dx=p[1]==6?1:p[1]==7?-1:0;
            } else { out.type=MGX_EVENT_BUTTON; out.code=p[1]-1; out.down=type==4; }
        } else out.type=MGX_EVENT_MOTION;
    } else if(type==22) {
        out.type=MGX_EVENT_RESIZE; out.window=mx11_get32(p+8);
        out.x=mx11_get16(p+20); out.y=mx11_get16(p+22);
        if(!out.x||!out.y) { errno=EPROTO; return -1; }
    } else if(type==12) { out.type=MGX_EVENT_FRAME; out.window=mx11_get32(p+4); }
    else if(type==17) { out.type=MGX_EVENT_CLOSE; out.window=mx11_get32(p+8); }
    else return 0;
    *e=out;
    return 1;
}
/* Legacy 32-byte API retains its historical keycode-minus-eight convention.
 * That convention is not a portable keyboard map; prefer the bounded API. */
static inline int mulib_x11_events_decode(const unsigned char p[32],mgx_event *e) {
    if(!p||!e)return 0;
    memset(e,0,sizeof *e);
    if(mulib_x11_events_decode_bounded(p,32,e)!=1)return 0;
    if(e->type==MGX_EVENT_KEY)e->code=e->code>=8?e->code-8:0;
    return 1;
}

enum {
    MULIB_X11_DESTROY_NOTIFY=17, MULIB_X11_UNMAP_NOTIFY=18,
    MULIB_X11_MAP_NOTIFY=19, MULIB_X11_MAP_REQUEST=20,
    MULIB_X11_REPARENT_NOTIFY=21, MULIB_X11_CONFIGURE_NOTIFY=22,
    MULIB_X11_CONFIGURE_REQUEST=23, MULIB_X11_PROPERTY_NOTIFY=28,
    MULIB_X11_CLIENT_MESSAGE=33
};
enum {
    MULIB_X11_CW_X=1u<<0, MULIB_X11_CW_Y=1u<<1,
    MULIB_X11_CW_WIDTH=1u<<2, MULIB_X11_CW_HEIGHT=1u<<3,
    MULIB_X11_CW_BORDER=1u<<4, MULIB_X11_CW_SIBLING=1u<<5,
    MULIB_X11_CW_STACK_MODE=1u<<6
};
typedef struct {
    uint32_t event,window,parent,sibling,atom,time;
    int16_t x,y;
    uint16_t width,height,border,mask;
    uint8_t type,synthetic,override_redirect,detail;
    unsigned char data[20]; /* ClientMessage payload in wire byte order. */
} mulib_x11_wm_event;
/* Reusable core-event decoder. 1 decoded, 0 unsupported, -1 malformed.
 * Synthetic events are identified here, not treated as server state. */
static inline int mulib_x11_events_wm(const unsigned char *p,size_t n,mulib_x11_wm_event *out) {
    if(!p||!out||n<32) { errno=EINVAL; return -1; }
    mulib_x11_wm_event e={0};
    e.type=p[0]&127;
    e.synthetic=(p[0]>>7);
    switch(e.type) {
    case MULIB_X11_DESTROY_NOTIFY:
    case MULIB_X11_UNMAP_NOTIFY:
    case MULIB_X11_MAP_NOTIFY:
    case MULIB_X11_MAP_REQUEST:
    case MULIB_X11_REPARENT_NOTIFY:
    case MULIB_X11_CONFIGURE_NOTIFY:
    case MULIB_X11_CONFIGURE_REQUEST:
        e.event=mx11_get32(p+4); e.window=mx11_get32(p+8);
        if(e.type==MULIB_X11_UNMAP_NOTIFY)e.detail=p[12]; /* from-configure */
        if(e.type==MULIB_X11_MAP_NOTIFY)e.override_redirect=p[12];
        if(e.type==MULIB_X11_REPARENT_NOTIFY) {
            e.parent=mx11_get32(p+12);
            e.x=(int16_t)mx11_get16(p+16); e.y=(int16_t)mx11_get16(p+18);
            e.override_redirect=p[20];
        }
        if(e.type==MULIB_X11_CONFIGURE_NOTIFY||e.type==MULIB_X11_CONFIGURE_REQUEST) {
            e.sibling=mx11_get32(p+12);
            e.x=(int16_t)mx11_get16(p+16); e.y=(int16_t)mx11_get16(p+18);
            e.width=mx11_get16(p+20); e.height=mx11_get16(p+22);
            e.border=mx11_get16(p+24);
            if(e.type==MULIB_X11_CONFIGURE_NOTIFY)e.override_redirect=p[26];
            else { e.detail=p[1]; e.mask=mx11_get16(p+26); }
        }
        break;
    case MULIB_X11_PROPERTY_NOTIFY:
        e.window=mx11_get32(p+4); e.atom=mx11_get32(p+8);
        e.time=mx11_get32(p+12); e.detail=p[16];
        break;
    case MULIB_X11_CLIENT_MESSAGE:
        e.window=mx11_get32(p+4); e.atom=mx11_get32(p+8); e.detail=p[1];
        if(e.detail!=8&&e.detail!=16&&e.detail!=32) { errno=EPROTO; return -1; }
        memcpy(e.data,p+12,sizeof e.data);
        break;
    default: return 0;
    }
    *out=e;
    return 1;
}
#endif
