/* MuLIB browser staging addition: fetch-credentials
 * Implemented subset: Credentials inclusion predicate for omit/same-origin/include.
 * Remaining: Cookie, TLS client certificate and authentication integration.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_FETCH_CREDENTIALS_H
#define MULIB_FETCH_CREDENTIALS_H
#include "detail/mulib-browser-additions-common.h"

typedef enum {mulib_fetch_credentials_OMIT,mulib_fetch_credentials_SAME_ORIGIN,mulib_fetch_credentials_INCLUDE} mulib_fetch_credentials_mode;
static inline int mulib_fetch_credentials_send(mulib_fetch_credentials_mode m,int same){return m==mulib_fetch_credentials_INCLUDE||(m==mulib_fetch_credentials_SAME_ORIGIN&&same);}
#endif /* MULIB_FETCH_CREDENTIALS_H */
