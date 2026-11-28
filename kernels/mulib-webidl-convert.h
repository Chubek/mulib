/* MuLIB browser staging addition: webidl-convert
 * Implemented subset: Modulo signed/unsigned integer conversion, clamp ties-to-even and boolean.
 * Remaining: ECMAScript ToPrimitive/ToNumber, strings, dictionaries and overload integration.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEBIDL_CONVERT_H
#define MULIB_WEBIDL_CONVERT_H
#include "detail/mulib-browser-additions-common.h"

static inline uint32_t mulib_webidl_convert_uint32(double x){if(!isfinite(x)||x==0)return 0;x=fmod(trunc(x),4294967296.0);if(x<0)x+=4294967296.0;return (uint32_t)x;}
static inline int32_t mulib_webidl_convert_int32(double x){uint32_t u=mulib_webidl_convert_uint32(x);return u<=INT32_MAX?(int32_t)u:(int32_t)((int64_t)u-4294967296LL);}
static inline unsigned char mulib_webidl_convert_clamp_octet(double x){if(isnan(x)||x<=0)return 0;if(x>=255)return 255;double f=floor(x),d=x-f;return (unsigned char)(f+(d>0.5||(d==0.5&&fmod(f,2)!=0)));}
#endif /* MULIB_WEBIDL_CONVERT_H */
