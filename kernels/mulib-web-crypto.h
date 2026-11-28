/* MuLIB browser staging addition: web-crypto
 * Implemented subset: Explicit entropy provider dispatch with Web Crypto getRandomValues byte limit.
 * Remaining: Secure provider, SubtleCrypto, keys, algorithms and JS typed arrays.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEB_CRYPTO_H
#define MULIB_WEB_CRYPTO_H
#include "detail/mulib-browser-additions-common.h"

typedef int (*mulib_web_crypto_entropy)(void *,unsigned char *,size_t);
static inline int mulib_web_crypto_random(mulib_web_crypto_entropy fn,void *ctx,unsigned char *out,size_t n){if(!fn||(!out&&n)||n>65536)return 0;return n?fn(ctx,out,n):1;}
#endif /* MULIB_WEB_CRYPTO_H */
