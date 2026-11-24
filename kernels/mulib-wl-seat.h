/* MuLIB notebook desktop extension. C11, header-only.
 * SPDX-License-Identifier: MIT
 * See AGENTS.md for integration and coverage limits. */
#ifndef MULIB_WL_SEAT_H
#define MULIB_WL_SEAT_H
#include "mulib-wl-client.h"
typedef struct {uint64_t keyboard,pointer;uint32_t serial,buttons;} mulib_wl_seat;
static inline uint32_t mulib_wl_seat_serial(mulib_wl_seat *s){if(!s)return 0;if(!++s->serial)++s->serial;return s->serial;}
static inline int mulib_wl_seat_fixed(double x,int32_t *out){if(!out||!isfinite(x)||x<-8388608.0||x>8388607.99609375)return 0;*out=(int32_t)(x*256);return 1;}
static inline int mulib_wl_seat_motion(mulib_wl_client *c,uint32_t pointer,uint32_t time,double x,double y){int32_t a,b;if(!mulib_wl_seat_fixed(x,&a)||!mulib_wl_seat_fixed(y,&b))return 0;uint32_t p[]={time,(uint32_t)a,(uint32_t)b};return mulib_wl_client_event(c,pointer,2,p,12);}
static inline int mulib_wl_seat_key(mulib_wl_client *c,uint32_t keyobj,uint32_t serial,uint32_t time,uint32_t key,int pressed){uint32_t p[]={serial,time,key,!!pressed};return serial&&mulib_wl_client_event(c,keyobj,3,p,16);}
static inline int mulib_wl_seat_button(mulib_wl_client *c,uint32_t ptr,uint32_t serial,uint32_t time,uint32_t button,int pressed){uint32_t p[]={serial,time,button,!!pressed};return serial&&mulib_wl_client_event(c,ptr,3,p,16);}
#endif
