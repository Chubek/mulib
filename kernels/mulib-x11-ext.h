/* MuLIB notebook desktop extension. C11, header-only.
 * SPDX-License-Identifier: MIT
 * See AGENTS.md for integration and coverage limits. */
#ifndef MULIB_X11_EXT_H
#define MULIB_X11_EXT_H
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "mulib-x11-wire.h"
/* Native little-endian X11 request adapter. Callback consumes body before returning.
 * Reply callback must match the exact request sequence while retaining unrelated events. */
typedef int (*mulib_x11_send_fn)(void *,uint8_t,uint8_t,const void *,size_t);
typedef int (*mulib_x11_reply_fn)(void *,uint8_t,uint8_t,const void *,size_t,unsigned char **,size_t *);
typedef struct {void *user;mulib_x11_send_fn send;mulib_x11_reply_fn reply;} mulib_x11_wm_io;
/* Adapter for a dedicated mx11_stream. The legacy callback shape is boolean;
 * inspect stream->error.code for X11 rejection and connection->failed for a
 * broken stream. Do not mix these helpers with unchecked legacy requests. */
static inline int mulib_x11_wm_wire_send(void *s,uint8_t op,uint8_t arg,const void *p,size_t n) {
    return mulib_x11_wire_checked((mx11_stream *)s,op,arg,p,n)==1;
}
static inline int mulib_x11_wm_wire_reply(void *s,uint8_t op,uint8_t arg,const void *p,size_t n,
        unsigned char **reply,size_t *reply_n) {
    return mulib_x11_wire_reply((mx11_stream *)s,op,arg,p,n,reply,reply_n)==1;
}
static inline mulib_x11_wm_io mulib_x11_wm_wire_io(mx11_stream *s) {
    return (mulib_x11_wm_io){s,mulib_x11_wm_wire_send,mulib_x11_wm_wire_reply};
}
typedef struct {uint8_t present,opcode,event,error;} mulib_x11_extension;
static inline void mulib_x11_wm_u16(unsigned char *p,uint16_t v){p[0]=(unsigned char)v;p[1]=(unsigned char)(v>>8);}
static inline void mulib_x11_wm_u32(unsigned char *p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(unsigned char)(v>>(8*i));}
static inline uint16_t mulib_x11_wm_r16(const unsigned char *p){return (uint16_t)(p[0]|p[1]<<8);}
static inline uint32_t mulib_x11_wm_r32(const unsigned char *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static inline int mulib_x11_wm_send(mulib_x11_wm_io *io,uint8_t op,uint8_t minor,const void *p,size_t n){return io&&io->send&&op&&!(n%4)&&n<=262124&&(!n||p)&&io->send(io->user,op,minor,p,n);}
static inline int mulib_x11_wm_reply(mulib_x11_wm_io *io,uint8_t op,uint8_t minor,const void *p,size_t n,unsigned char **out,size_t *len){if(!io||!io->reply||!out||!len||!op||n%4||n>262124||(!p&&n))return 0;unsigned char *r=NULL;size_t z=0;if(!io->reply(io->user,op,minor,p,n,&r,&z)||!r||z<32||r[0]!=1||(uint64_t)mulib_x11_wm_r32(r+4)*4+32!=z){free(r);return 0;}*out=r;*len=z;return 1;}
static inline int mulib_x11_ext_query(mulib_x11_wm_io *io,const char *name,mulib_x11_extension *e){if(!name||!e)return 0;size_t n=strlen(name);if(!n||n>65535)return 0;size_t z=4+(n+3)/4*4;unsigned char *p=(unsigned char*)calloc(z,1),*r;size_t rn;if(!p)return 0;mulib_x11_wm_u16(p,(uint16_t)n);memcpy(p+4,name,n);int ok=mulib_x11_wm_reply(io,98,0,p,z,&r,&rn);free(p);if(!ok)return 0;*e=(mulib_x11_extension){r[8],r[9],r[10],r[11]};free(r);return 1;}
static inline int mulib_x11_ext_version(mulib_x11_wm_io *io,mulib_x11_extension e,uint32_t major,uint32_t minor,uint32_t *a,uint32_t *b){if(!e.present||!a||!b)return 0;unsigned char p[8],*r;size_t n;mulib_x11_wm_u32(p,major);mulib_x11_wm_u32(p+4,minor);if(!mulib_x11_wm_reply(io,e.opcode,0,p,8,&r,&n))return 0;*a=mulib_x11_wm_r32(r+8);*b=mulib_x11_wm_r32(r+12);free(r);return 1;}
#endif
