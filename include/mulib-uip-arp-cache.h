/** @file mulib-uip-arp-cache.h
 * @brief Small time-bounded IPv4-to-MAC ARP cache with deterministic eviction
 * C11, single-header, define MULIB_IMPL_UIP_ARP_CACHE exactly once.
 */
#ifndef MULIB_UIP_ARP_CACHE_H
#define MULIB_UIP_ARP_CACHE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_uip_arp_entry
  {
    uint8_t ip[4], mac[6];
    uint64_t expires_at;
    int valid;
  } mulib_uip_arp_entry_t;
  typedef struct mulib_uip_arp_cache
  {
    mulib_uip_arp_entry_t entries[32];
  } mulib_uip_arp_cache_t;
  int mulib_uip_arp_cache_store (mulib_uip_arp_cache_t *cache,
                                 const uint8_t ip[4], const uint8_t mac[6],
                                 uint64_t now, uint64_t lifetime);
  int mulib_uip_arp_cache_find (const mulib_uip_arp_cache_t *cache,
                                const uint8_t ip[4], uint64_t now,
                                uint8_t mac[6]);
  void mulib_uip_arp_cache_expire (mulib_uip_arp_cache_t *cache, uint64_t now);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_UIP_ARP_CACHE)                                         \
    && !defined(MULIB_UIP_ARP_CACHE_IMPLEMENTED)
#define MULIB_UIP_ARP_CACHE_IMPLEMENTED
#include <errno.h>
#include <string.h>
int
mulib_uip_arp_cache_store (mulib_uip_arp_cache_t *c, const uint8_t ip[4],
                           const uint8_t mac[6], uint64_t now, uint64_t life)
{
  if (!c || !ip || !mac || !life || UINT64_MAX - now < life)
    return -EINVAL;
  int slot = 0, available = -1;
  uint64_t earliest = UINT64_MAX;
  for (unsigned i = 0; i < 32; i++)
    {
      mulib_uip_arp_entry_t *e = &c->entries[i];
      if (e->valid && memcmp (e->ip, ip, 4) == 0)
        {
          slot = (int)i;
          break;
        }
      if (!e->valid || e->expires_at <= now)
        {
          if (available < 0)
            available = (int)i;
        }
      if (e->expires_at < earliest)
        {
          earliest = e->expires_at;
          slot = (int)i;
        }
      if (i == 31 && available >= 0)
        slot = available;
    }
  mulib_uip_arp_entry_t *e = &c->entries[slot];
  memcpy (e->ip, ip, 4);
  memcpy (e->mac, mac, 6);
  e->expires_at = now + life;
  e->valid = 1;
  return 0;
}
int
mulib_uip_arp_cache_find (const mulib_uip_arp_cache_t *c, const uint8_t ip[4],
                          uint64_t now, uint8_t mac[6])
{
  if (!c || !ip || !mac)
    return -EINVAL;
  for (size_t i = 0; i < 32; i++)
    {
      const mulib_uip_arp_entry_t *e = &c->entries[i];
      if (e->valid && e->expires_at > now && !memcmp (e->ip, ip, 4))
        {
          memcpy (mac, e->mac, 6);
          return 0;
        }
    }
  return -ENOENT;
}
void
mulib_uip_arp_cache_expire (mulib_uip_arp_cache_t *c, uint64_t now)
{
  if (!c)
    return;
  for (size_t i = 0; i < 32; i++)
    if (c->entries[i].valid && c->entries[i].expires_at <= now)
      c->entries[i].valid = 0;
}
#endif
