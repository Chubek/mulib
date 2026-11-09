/** @file mulib-uip-pbuf.h
 * @brief Owned dynamically allocated packet buffer supporting prepend and
 * append C11, single-header, define MULIB_IMPL_UIP_PBUF exactly once.
 */
#ifndef MULIB_UIP_PBUF_H
#define MULIB_UIP_PBUF_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_uip_pbuf
  {
    uint8_t *memory;
    size_t capacity, start, length;
  } mulib_uip_pbuf_t;
  int mulib_uip_pbuf_init (mulib_uip_pbuf_t *p, size_t headroom,
                           size_t capacity);
  void mulib_uip_pbuf_destroy (mulib_uip_pbuf_t *p);
  int mulib_uip_pbuf_prepend (mulib_uip_pbuf_t *p, const void *data, size_t n);
  int mulib_uip_pbuf_append (mulib_uip_pbuf_t *p, const void *data, size_t n);
  const void *mulib_uip_pbuf_data (const mulib_uip_pbuf_t *p);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_UIP_PBUF) && !defined(MULIB_UIP_PBUF_IMPLEMENTED)
#define MULIB_UIP_PBUF_IMPLEMENTED
#include <errno.h>
#include <stdlib.h>
#include <string.h>
int
mulib_uip_pbuf_init (mulib_uip_pbuf_t *p, size_t head, size_t cap)
{
  if (!p || p->memory || head > cap || cap > 65536)
    return -EINVAL;
  uint8_t *b = malloc (cap ? cap : 1);
  if (!b)
    return -ENOMEM;
  p->memory = b;
  p->capacity = cap;
  p->start = head;
  p->length = 0;
  return 0;
}
void
mulib_uip_pbuf_destroy (mulib_uip_pbuf_t *p)
{
  if (p)
    {
      free (p->memory);
      memset (p, 0, sizeof *p);
    }
}
int
mulib_uip_pbuf_prepend (mulib_uip_pbuf_t *p, const void *d, size_t n)
{
  if (!p || !p->memory || (!d && n) || p->start > p->capacity
      || p->length > p->capacity - p->start || n > p->start)
    return -EINVAL;
  p->start -= n;
  p->length += n;
  if (n)
    memcpy (p->memory + p->start, d, n);
  return 0;
}
int
mulib_uip_pbuf_append (mulib_uip_pbuf_t *p, const void *d, size_t n)
{
  if (!p || !p->memory || (!d && n) || p->start > p->capacity
      || p->length > p->capacity - p->start
      || n > p->capacity - p->start - p->length)
    return -EINVAL;
  if (n)
    memcpy (p->memory + p->start + p->length, d, n);
  p->length += n;
  return 0;
}
const void *
mulib_uip_pbuf_data (const mulib_uip_pbuf_t *p)
{
  return p && p->memory && p->start <= p->capacity
                 && p->length <= p->capacity - p->start
             ? p->memory + p->start
             : NULL;
}
#endif
