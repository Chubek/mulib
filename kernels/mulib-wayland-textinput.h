/* MuLIB graphical extension: configured text-input-v3 requests. C11. */
#ifndef MULIB_WAYLAND_TEXTINPUT_H
#define MULIB_WAYLAND_TEXTINPUT_H
#include "mulib-wayland-wire.h"
static inline int mulib_wayland_textinput_enable(int fd,uint32_t object,int enable) {
    return mulib_wayland_wire_send(fd,object,enable?1:2,NULL,0,-1)==0;
}
/* Validate scalar UTF-8 and byte offsets; no shaping/IME implementation implied. */
static inline int mulib_wayland_textinput_text_valid(const char *text,size_t n,
        uint32_t cursor,uint32_t anchor) {
    if(!text||n>4000||cursor>n||anchor>n)return 0;
    size_t i=0;
    int cb=cursor==0,ab=anchor==0;
    while(i<n) {
        unsigned char c=(unsigned char)text[i++];
        uint32_t value,minimum;
        unsigned count;
        if(c<0x80) { if(!c)return 0; value=c; minimum=0; count=0; }
        else if(c>=0xc2&&c<=0xdf) { value=c&31u; minimum=0x80; count=1; }
        else if(c>=0xe0&&c<=0xef) { value=c&15u; minimum=0x800; count=2; }
        else if(c>=0xf0&&c<=0xf4) { value=c&7u; minimum=0x10000; count=3; }
        else return 0;
        while(count--) {
            if(i==n)return 0;
            c=(unsigned char)text[i++];
            if((c&0xc0u)!=0x80u)return 0;
            value=(value<<6)|(c&63u);
        }
        if(value<minimum||value>0x10ffffu||(value>=0xd800u&&value<=0xdfffu))return 0;
        if(i==cursor)cb=1;
        if(i==anchor)ab=1;
    }
    return cb&&ab;
}
static inline int mulib_wayland_textinput_surrounding(int fd,uint32_t object,
        const char *text,uint32_t cursor,uint32_t anchor) {
    if(!text) { errno=EINVAL; return 0; }
    size_t n=strlen(text);
    if(!mulib_wayland_textinput_text_valid(text,n,cursor,anchor)) { errno=EINVAL; return 0; }
    mway_bytes b={0};
    int ok=mway_string(&b,text)&&mway_u32(&b,cursor)&&mway_u32(&b,anchor)&&
           mulib_wayland_wire_send(fd,object,3,b.v,b.n,-1)==0;
    mway_bytes_free(&b);
    return ok;
}
static inline int mulib_wayland_textinput_commit(int fd,uint32_t object) {
    return mulib_wayland_wire_send(fd,object,7,NULL,0,-1)==0;
}
#endif
