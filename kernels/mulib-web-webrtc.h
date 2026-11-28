/* MuLIB browser staging addition: web-webrtc
 * Implemented subset: Offer/answer signaling transition validation (no networking).
 * Remaining: ICE, STUN/TURN, DTLS/SRTP, SDP, negotiation and data channels.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEB_WEBRTC_H
#define MULIB_WEB_WEBRTC_H
#include "detail/mulib-browser-additions-common.h"

typedef enum {mulib_web_webrtc_STABLE,mulib_web_webrtc_LOCAL_OFFER,mulib_web_webrtc_REMOTE_OFFER,mulib_web_webrtc_CLOSED} mulib_web_webrtc_state;
static inline int mulib_web_webrtc_offer(mulib_web_webrtc_state *s,int local){if(!s||*s!=mulib_web_webrtc_STABLE)return 0;*s=local?mulib_web_webrtc_LOCAL_OFFER:mulib_web_webrtc_REMOTE_OFFER;return 1;}
static inline int mulib_web_webrtc_answer(mulib_web_webrtc_state *s,int local){if(!s||*s!=(local?mulib_web_webrtc_REMOTE_OFFER:mulib_web_webrtc_LOCAL_OFFER))return 0;*s=mulib_web_webrtc_STABLE;return 1;}
#endif /* MULIB_WEB_WEBRTC_H */
