/** @file mulib-uip-route.h
 * @brief Fixed-table longest-prefix matching for userspace IPv4 routing
 * C11, single-header, define MULIB_IMPL_UIP_ROUTE exactly once.
 */
#ifndef MULIB_UIP_ROUTE_H
#define MULIB_UIP_ROUTE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_uip_route_entry
  {
    uint8_t prefix[4], gateway[4];
    uint8_t bits;
    unsigned interface_id;
    int valid;
  } mulib_uip_route_entry_t;
  typedef struct mulib_uip_routes
  {
    mulib_uip_route_entry_t entries[16];
  } mulib_uip_routes_t;
  int mulib_uip_route_add (mulib_uip_routes_t *table, const uint8_t prefix[4],
                           unsigned bits, const uint8_t gateway[4],
                           unsigned ifid);
  int mulib_uip_route_find (const mulib_uip_routes_t *table,
                            const uint8_t address[4],
                            mulib_uip_route_entry_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_UIP_ROUTE) && !defined(MULIB_UIP_ROUTE_IMPLEMENTED)
#define MULIB_UIP_ROUTE_IMPLEMENTED
#include <errno.h>
#include <string.h>
static int
mulib_uip_route_match_ (const uint8_t a[4], const uint8_t p[4], unsigned bits)
{
  for (unsigned i = 0; i < 4; i++)
    {
      unsigned n = bits > 8 ? 8 : bits;
      uint8_t mask = n ? (uint8_t)(0xffu << (8 - n)) : 0;
      if ((a[i] & mask) != (p[i] & mask))
        return 0;
      bits -= n;
    }
  return 1;
}
int
mulib_uip_route_add (mulib_uip_routes_t *t, const uint8_t ip[4], unsigned bits,
                     const uint8_t gw[4], unsigned ifid)
{
  if (!t || !ip || bits > 32 || !ifid)
    return -EINVAL;
  for (size_t i = 0; i < 16; i++)
    if (!t->entries[i].valid)
      {
        mulib_uip_route_entry_t *e = &t->entries[i];
        memcpy (e->prefix, ip, 4);
        if (gw)
          memcpy (e->gateway, gw, 4);
        else
          memset (e->gateway, 0, 4);
        e->bits = (uint8_t)bits;
        e->interface_id = ifid;
        e->valid = 1;
        return 0;
      }
  return -ENOSPC;
}
int
mulib_uip_route_find (const mulib_uip_routes_t *t, const uint8_t addr[4],
                      mulib_uip_route_entry_t *out)
{
  if (!t || !addr || !out)
    return -EINVAL;
  int best = -1;
  for (unsigned i = 0; i < 16; i++)
    if (t->entries[i].valid
        && mulib_uip_route_match_ (addr, t->entries[i].prefix,
                                   t->entries[i].bits))
      if (best < 0 || t->entries[i].bits > t->entries[best].bits)
        best = (int)i;
  if (best < 0)
    return -ENOENT;
  *out = t->entries[best];
  return 0;
}
#endif
