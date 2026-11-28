/* MuLIB browser staging addition: http2-session
 * Implemented subset: Checked flow control window accounting and stream ID allocation.
 * Remaining: Complete connection/stream state, SETTINGS, HPACK, TLS transport.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_HTTP2_SESSION_H
#define MULIB_HTTP2_SESSION_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {int64_t send_window;uint32_t next_id;} mulib_http2_session_t;
static inline int mulib_http2_session_consume(mulib_http2_session_t *s,uint32_t n){if(!s||s->send_window<(int64_t)n)return 0;s->send_window-=n;return 1;}
static inline int mulib_http2_session_update(mulib_http2_session_t *s,uint32_t n){if(!s||!n||n>0x7fffffffu||s->send_window>INT32_MAX-(int64_t)n)return 0;s->send_window+=n;return 1;}
static inline uint32_t mulib_http2_session_open(mulib_http2_session_t *s){if(!s||!s->next_id||s->next_id>0x7fffffffu)return 0;uint32_t id=s->next_id;s->next_id+=2;return id;}
#endif /* MULIB_HTTP2_SESSION_H */
