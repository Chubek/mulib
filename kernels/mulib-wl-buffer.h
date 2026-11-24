/* MuLIB notebook desktop extension. C11, header-only.
 * SPDX-License-Identifier: MIT
 * See AGENTS.md for integration and coverage limits. */
#ifndef MULIB_WL_BUFFER_H
#define MULIB_WL_BUFFER_H
#include "mulib-glx-clientbuffer.h"
/* State machine: resource owner and each pending/current use retain a ref. */
typedef struct {mulib_glx_clientbuffer image;unsigned uses;int destroyed;void (*idle)(void *);void *user;} mulib_wl_buffer;
static inline int mulib_wl_buffer_acquire(mulib_wl_buffer *b){if(!b||b->destroyed||b->uses==UINT_MAX||!mulib_glx_clientbuffer_ref(&b->image))return 0;b->uses++;return 1;}
static inline void mulib_wl_buffer_release(mulib_wl_buffer *b){if(b&&b->uses){b->uses--;if(!b->uses&&!b->destroyed&&b->idle)b->idle(b->user);mulib_glx_clientbuffer_unref(&b->image);}}
static inline void mulib_wl_buffer_destroy(mulib_wl_buffer *b){if(b&&!b->destroyed){b->destroyed=1;mulib_glx_clientbuffer_unref(&b->image);}}
#endif
