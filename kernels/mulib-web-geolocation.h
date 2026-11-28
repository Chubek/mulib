/* MuLIB browser staging addition: web-geolocation
 * Implemented subset: Validate provider-supplied latitude/longitude/accuracy and permission gate.
 * Remaining: Asynchronous watches, timeout, privacy and permission broker.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEB_GEOLOCATION_H
#define MULIB_WEB_GEOLOCATION_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {double latitude,longitude,accuracy;} mulib_web_geolocation_position;
typedef int (*mulib_web_geolocation_provider)(void *,mulib_web_geolocation_position *);
static inline int mulib_web_geolocation_get(int permitted,mulib_web_geolocation_provider fn,void *ctx,mulib_web_geolocation_position *out){if(!permitted||!fn||!out)return 0;mulib_web_geolocation_position p={NAN,NAN,NAN};if(!fn(ctx,&p)||!isfinite(p.latitude)||!isfinite(p.longitude)||!isfinite(p.accuracy)||fabs(p.latitude)>90||fabs(p.longitude)>180||p.accuracy<0)return 0;*out=p;return 1;}
#endif /* MULIB_WEB_GEOLOCATION_H */
