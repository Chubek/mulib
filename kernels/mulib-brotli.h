/* MuLIB browser staging addition: brotli
 * Implemented subset: Bounded Brotli window-bit header decoder.
 * Remaining: Meta-block decoder, Huffman, dictionary, transforms and decompression.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_BROTLI_H
#define MULIB_BROTLI_H
#include "detail/mulib-browser-additions-common.h"

static inline int mulib_brotli_window_bits(muba_span s,unsigned *bits){if(!bits||!muba_span_ok(s)||!s.len)return 0;unsigned v=s.data[0];if(!(v&1)){*bits=16;return 1;}unsigned n=(v>>1)&7;if(n){*bits=17+n;return 1;}n=(v>>4)&7;if(n==1)return 0;*bits=n?8+n:17;return 1;}
#endif /* MULIB_BROTLI_H */
