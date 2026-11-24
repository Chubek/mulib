/** @file mulib-uip-fragment.h
 * @brief One-datagram IPv4 payload reassembly with overlap rejection and
 * bounded memory C11, single-header, define MULIB_IMPL_UIP_FRAGMENT exactly
 * once.
 */
#ifndef MULIB_UIP_FRAGMENT_H
#define MULIB_UIP_FRAGMENT_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_uip_frag
  {
    uint8_t bytes[8192], occupied[8192];
    uint16_t id;
    uint8_t src[4], dst[4], protocol;
    size_t final_size;
    int initialized, has_last;
  } mulib_uip_frag_t;
  void mulib_uip_frag_reset (mulib_uip_frag_t *r);
  int mulib_uip_frag_push (mulib_uip_frag_t *r, const uint8_t src[4],
                           const uint8_t dst[4], uint8_t protocol, uint16_t id,
                           size_t offset, int more, const void *data, size_t n,
                           size_t *complete_size);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_UIP_FRAGMENT)                                          \
    && !defined(MULIB_UIP_FRAGMENT_IMPLEMENTED)
#define MULIB_UIP_FRAGMENT_IMPLEMENTED
#include <errno.h>
#include <string.h>
void
mulib_uip_frag_reset (mulib_uip_frag_t *r)
{
  if (r)
    memset (r, 0, sizeof *r);
}
int
mulib_uip_frag_push (mulib_uip_frag_t *r, const uint8_t src[4],
                     const uint8_t dst[4], uint8_t proto, uint16_t id,
                     size_t off, int more, const void *p, size_t n,
                     size_t *complete)
{
  if (!r || !src || !dst || !p || !complete || !n || (more != 0 && more != 1)
      || off % 8 || (more && n % 8) || off > 8192 || n > 8192 - off)
    return -EINVAL;
  *complete = 0;
  if (!r->initialized)
    {
      r->initialized = 1;
      r->id = id;
      r->protocol = proto;
      memcpy (r->src, src, 4);
      memcpy (r->dst, dst, 4);
    }
  else if (r->id != id || r->protocol != proto || memcmp (r->src, src, 4)
           || memcmp (r->dst, dst, 4))
    return -EINVAL;
  if (!more)
    {
      if (r->has_last && r->final_size != off + n)
        return -EBADMSG;
      for (size_t i = off + n; i < sizeof r->occupied; i++)
        if (r->occupied[i])
          return -EBADMSG;
    }
  if (r->has_last && off + n > r->final_size)
    return -EBADMSG;
  for (size_t i = off; i < off + n; i++)
    if (r->occupied[i])
      return -EEXIST;
  if (!more)
    {
      r->has_last = 1;
      r->final_size = off + n;
    }
  if (n)
    memcpy (r->bytes + off, p, n);
  memset (r->occupied + off, 1, n);
  if (r->has_last)
    {
      for (size_t i = 0; i < r->final_size; i++)
        if (!r->occupied[i])
          return 0;
      *complete = r->final_size;
      return 1;
    }
  return 0;
}
#endif
