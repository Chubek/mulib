/** @file mulib-abstun.h
 * @brief Abstract packet tunnel with checked MTU and packet-atomic callbacks.
 * Define MULIB_IMPL_ABSTUN once. Backend/context are borrowed.
 */
#ifndef MULIB_ABSTUN_H
#define MULIB_ABSTUN_H
#include <stddef.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_abstun
  {
    void *context;
    size_t mtu;
    int (*receive) (void *, void *, size_t, size_t *);
    int (*send) (void *, const void *, size_t);
  } mulib_abstun_t;
  /** @brief Return 0 packet, 1 would-block, negative error. Receive capacity
   * must be >= MTU; successful packets have length 1..MTU. No EOF packets. */
  int mulib_abstun_receive (const mulib_abstun_t *, void *, size_t, size_t *);
  /** @brief Send one complete packet; reject empty/oversize packets before
   * I/O. */
  int mulib_abstun_send (const mulib_abstun_t *, const void *, size_t);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_ABSTUN) && !defined(MULIB_ABSTUN_IMPLEMENTED)
#define MULIB_ABSTUN_IMPLEMENTED
int
mulib_abstun_receive (const mulib_abstun_t *s, void *p, size_t n,
                      size_t *count)
{
  size_t got = 0;
  int r;
  if (count)
    *count = 0;
  if (!s || !s->receive || !s->mtu || !p || !count || n < s->mtu)
    return -1;
  r = s->receive (s->context, p, s->mtu, &got);
  if (got > s->mtu || (r && got) || (!r && !got) || r > 1)
    return -2;
  if (!r)
    *count = got;
  return r;
}
int
mulib_abstun_send (const mulib_abstun_t *s, const void *p, size_t n)
{
  int r;
  if (!s || !s->send || !s->mtu || !p || !n || n > s->mtu)
    return -1;
  r = s->send (s->context, p, n);
  return r > 1 ? -2 : r;
}
#endif
