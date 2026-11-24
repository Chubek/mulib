/** @file mulib-tunnel.h
 * @brief Backpressure-preserving, bounded duplex stream relay.
 * Define MULIB_IMPL_TUNNEL and MULIB_IMPL_XTKS once.
 */
#ifndef MULIB_TUNNEL_H
#define MULIB_TUNNEL_H
#include "mulib-xtks.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_tunnel_lane
  {
    unsigned char *buffer;
    size_t capacity, begin, end;
    int eof;
  } mulib_tunnel_lane_t;
  typedef struct mulib_tunnel
  {
    mulib_xtks_t peers[2];
    mulib_tunnel_lane_t lanes[2];
    int failed;
  } mulib_tunnel_t;
  /** @brief Borrow two streams and disjoint nonempty buffers. No close
   * ownership. */
  int mulib_tunnel_init (mulib_tunnel_t *, const mulib_xtks_t *,
                         const mulib_xtks_t *, void *, void *,
                         size_t capacity);
  /** @brief At most one read and write per direction. OK progress, AGAIN idle,
   * EOF both inputs ended and buffers drained, negative sticky failure. Caller
   * handles half-close policy; EOF in one direction does not discard the
   * other. */
  int mulib_tunnel_step (mulib_tunnel_t *);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_TUNNEL) && !defined(MULIB_TUNNEL_IMPLEMENTED)
#define MULIB_TUNNEL_IMPLEMENTED
#include <string.h>
int
mulib_tunnel_init (mulib_tunnel_t *t, const mulib_xtks_t *a,
                   const mulib_xtks_t *b, void *ab, void *ba, size_t cap)
{
  if (!t || !a || !b || !a->read || !a->write || !b->read || !b->write || !ab
      || !ba || ab == ba || !cap)
    return -1;
  memset (t, 0, sizeof (*t));
  t->peers[0] = *a;
  t->peers[1] = *b;
  t->lanes[0].buffer = ab;
  t->lanes[1].buffer = ba;
  t->lanes[0].capacity = t->lanes[1].capacity = cap;
  return 0;
}
int
mulib_tunnel_step (mulib_tunnel_t *t)
{
  int progress = 0, done = 0;
  if (!t)
    return -1;
  if (t->failed)
    return t->failed;
  for (unsigned i = 0; i < 2; ++i)
    {
      mulib_tunnel_lane_t *l = &t->lanes[i];
      size_t n;
      int r;
      if (!l->buffer || !l->capacity)
        return -1;
      if (l->begin == l->end)
        {
          l->begin = l->end = 0;
          if (!l->eof)
            {
              r = mulib_xtks_read (&t->peers[i], l->buffer, l->capacity, &n);
              if (r < 0)
                {
                  t->failed = r;
                  return r;
                }
              if (r == 2)
                {
                  l->eof = 1;
                  progress = 1;
                }
              if (!r)
                {
                  l->end = n;
                  progress = 1;
                }
            }
        }
      if (l->begin < l->end)
        {
          r = mulib_xtks_write (&t->peers[1 - i], l->buffer + l->begin,
                                l->end - l->begin, &n);
          if (r < 0)
            {
              t->failed = r;
              return r;
            }
          if (!r)
            {
              l->begin += n;
              progress = 1;
            }
        }
      if (l->eof && l->begin == l->end)
        ++done;
    }
  return done == 2 ? 2 : progress ? 0 : 1;
}
#endif
