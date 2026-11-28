/* MuLIB browser staging addition: web-mediastream
 * Implemented subset: Bounded track collection with unique IDs and enabled state.
 * Remaining: Capture, tracks, constraints, permissions and device lifecycle.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEB_MEDIASTREAM_H
#define MULIB_WEB_MEDIASTREAM_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {uint64_t id;unsigned kind;int enabled;} mulib_web_mediastream_track;
static inline int mulib_web_mediastream_add(mulib_web_mediastream_track *v,size_t *n,size_t cap,mulib_web_mediastream_track t){if(!v||!n||*n>=cap||!t.id)return 0;for(size_t i=0;i<*n;i++)if(v[i].id==t.id)return 0;v[(*n)++]=t;return 1;}
#endif /* MULIB_WEB_MEDIASTREAM_H */
