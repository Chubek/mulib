/** @file mulib-packetstream.h
 * @brief Incremental bounded four-byte big-endian length framing.
 * Define MULIB_IMPL_PACKETSTREAM and MULIB_IMPL_ENDIAN once.
 */
#ifndef MULIB_PACKETSTREAM_H
#define MULIB_PACKETSTREAM_H
#include "mulib-endian.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_packetstream
  {
    unsigned char header[4], *buffer;
    size_t header_used, used, length, capacity;
    int failed;
  } mulib_packetstream_t;
  /** @brief Initialize a decoder borrowing caller storage, capacity <=
   * UINT32_MAX. */
  int mulib_packetstream_init (mulib_packetstream_t *, void *storage,
                               size_t capacity);
  /** @brief Consume arbitrary fragments. Callback borrows each complete
   * payload (including empty packets). Returns 0 success, 1 callback stop, -1
   * invalid, -2 oversized packet (sticky). *consumed always reports accepted
   * input. Callback must not reenter or mutate the decoder. */
  int mulib_packetstream_feed (mulib_packetstream_t *, const void *, size_t,
                               size_t *consumed,
                               int (*packet) (void *, const void *, size_t),
                               void *);
  /** @brief Encode one packet; nonoverlapping output, 1 success, 0 failure. */
  int mulib_packetstream_encode (const void *, size_t, void *, size_t,
                                 size_t *);
  /** @brief Return 1 only at a clean frame boundary, suitable for checking
   * EOF. */
  int mulib_packetstream_finished (const mulib_packetstream_t *);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_PACKETSTREAM)                                          \
    && !defined(MULIB_PACKETSTREAM_IMPLEMENTED)
#define MULIB_PACKETSTREAM_IMPLEMENTED
#include <string.h>
int
mulib_packetstream_init (mulib_packetstream_t *s, void *p, size_t n)
{
  if (!s || (!p && n) || n > UINT32_MAX)
    return 0;
  memset (s, 0, sizeof (*s));
  s->buffer = p;
  s->capacity = n;
  return 1;
}
int
mulib_packetstream_feed (mulib_packetstream_t *s, const void *data, size_t n,
                         size_t *consumed,
                         int (*packet) (void *, const void *, size_t),
                         void *ctx)
{
  const unsigned char *p = data;
  size_t i = 0;
  if (consumed)
    *consumed = 0;
  if (!s || !consumed || !packet || (!p && n))
    return -1;
  if (s->failed)
    return -2;
  while (i < n)
    {
      if (s->header_used < 4)
        {
          s->header[s->header_used++] = p[i++];
          if (s->header_used < 4)
            continue;
          s->length = mulib_endian_load_be32 (s->header);
          if (s->length > s->capacity)
            {
              s->failed = 1;
              *consumed = i;
              return -2;
            }
        }
      if (s->used < s->length)
        {
          size_t take = s->length - s->used;
          if (take > n - i)
            take = n - i;
          memcpy (s->buffer + s->used, p + i, take);
          s->used += take;
          i += take;
        }
      if (s->used == s->length)
        {
          size_t length = s->length;
          s->header_used = s->used = s->length = 0;
          if (packet (ctx, s->buffer, length))
            {
              *consumed = i;
              return 1;
            }
        }
    }
  *consumed = i;
  return 0;
}
int
mulib_packetstream_encode (const void *p, size_t n, void *out, size_t cap,
                           size_t *used)
{
  if (used)
    *used = 0;
  if ((!p && n) || !out || !used || n > UINT32_MAX || cap < 4 || n > cap - 4)
    return 0;
  mulib_endian_store_be32 (out, (uint32_t)n);
  if (n)
    memcpy ((unsigned char *)out + 4, p, n);
  *used = n + 4;
  return 1;
}
int
mulib_packetstream_finished (const mulib_packetstream_t *s)
{
  return s && !s->failed && !s->header_used;
}
#endif
