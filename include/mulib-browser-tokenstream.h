/** @file mulib-browser-tokenstream.h
 * @brief HTML and CSS token event publishing.
 * C11 single-header module. Define MULIB_IMPL_BROWSER_TOKENSTREAM in exactly
 * one C translation unit.
 *
 */
#ifndef MULIB_BROWSER_TOKENSTREAM_H
#define MULIB_BROWSER_TOKENSTREAM_H
#include "mulib-browser-stream.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief See manual/mulib-browser-tokenstream.md for this operation's
   * contract. */
  int mulib_browser_tokenstream_emit (mulib_browser_stream_t *stream,
                                      unsigned kind, uint64_t node_id,
                                      const void *data, size_t length);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_BROWSER_TOKENSTREAM_H */
#ifdef MULIB_IMPL_BROWSER_TOKENSTREAM
#ifndef MULIB_IMPL_BROWSER_TOKENSTREAM_ONCE
#define MULIB_IMPL_BROWSER_TOKENSTREAM_ONCE
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int
mulib_browser_tokenstream_emit (mulib_browser_stream_t *s, unsigned k,
                                uint64_t id, const void *d, size_t n)
{
  return mulib_browser_stream_emit (s, MULIB_BROWSER_CHANNEL_TOKEN, k, id, d,
                                    n);
}
#endif
#endif
