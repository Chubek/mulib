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
#endif
