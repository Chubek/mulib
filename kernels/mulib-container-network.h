/** @file mulib-container-network.h
 * @brief IPv4/IPv6 routing and ordered packet policy over virtual devices.
 * Define MULIB_IMPL_CONTAINER_NETWORK plus NETDEV and PACKETRULES dependencies. */
#ifndef MULIB_CONTAINER_NETWORK_H
#define MULIB_CONTAINER_NETWORK_H
#include "mulib-container-netdev.h"
#include "mulib-packetrules.h"
#ifdef __cplusplus
extern "C" {
#endif
/** @brief Route owns prefix bytes and borrows an initialized endpoint. Ethernet
 * next-hop MACs are supplied explicitly; no ARP/NDP runs implicitly. */
typedef struct mulib_container_network_route
{
  unsigned family, prefix;
  unsigned char network[16], next_hop_mac[6];
  uint32_t metric;
  mulib_container_netdev *device;
} mulib_container_network_route;
typedef struct mulib_container_network
{
  mulib_container_network_route *routes;
  size_t count, max_routes;
  mulib_packetrules_rule_t *rules;
  size_t rule_count, max_rules;
  int default_action, busy;
  uint64_t accepted, denied, unroutable;
} mulib_container_network;
/** @brief Initialize empty routes and an explicit default accept/drop policy. */
int mulib_container_network_init (mulib_container_network *network,
                                  size_t max_routes, size_t max_rules, int default_action);
/** @brief Release routes/rules, never endpoints. */
void mulib_container_network_free (mulib_container_network *network);
/** @brief Copy a canonical route; duplicate family/prefix/network/metric fails. */
int mulib_container_network_add (mulib_container_network *network,
                                 const mulib_container_network_route *route);
/** @brief Remove route by index; indices after it shift down. */
int mulib_container_network_remove (mulib_container_network *network, size_t index);
/** @brief Atomically replace all ordered packet rules. */
int mulib_container_network_policy (mulib_container_network *network,
    const mulib_packetrules_rule_t *rules, size_t count, int default_action);
/** @brief Longest-prefix lookup, lower metric on ties; ignores down devices. */
const mulib_container_network_route *mulib_container_network_lookup (
    const mulib_container_network *network, unsigned family, const unsigned char *address);
/** @brief Validate/filter one L3 packet. Returns OK, EACCESS or EFORMAT. */
int mulib_container_network_check (mulib_container_network *network,
                                   const void *packet, size_t length, size_t *matched_rule);
/** @brief Filter, route and send one L3 packet; Ethernet routes add a 14-byte
 * header. No fragmentation, NAT, TTL rewriting or host network mutation. */
int mulib_container_network_send (mulib_container_network *network,
                                  const void *packet, size_t length);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CONTAINER_NETWORK) && !defined(MULIB_CONTAINER_NETWORK_IMPLEMENTED)
#define MULIB_CONTAINER_NETWORK_IMPLEMENTED
int mulib_container_network_init (mulib_container_network *n, size_t routes, size_t rules, int action)
{
  if (!n || !routes || routes > SIZE_MAX / sizeof (*n->routes) || rules > 65536
      || (action != 0 && action != 1)) return MULIB_CT_EINVAL;
  memset (n, 0, sizeof (*n)); n->max_routes = routes; n->max_rules = rules;
  n->default_action = action;
  return 0;
}
void mulib_container_network_free (mulib_container_network *n)
{
  if (!n || n->busy) return;
  MULIB_CT_FREE (n->routes); MULIB_CT_FREE (n->rules); memset (n, 0, sizeof (*n));
}
static int mulib_container_network_prefix_ (const unsigned char *a, const unsigned char *b, unsigned bits)
{
  unsigned i;
  for (i = 0; i < bits; ++i) if ((a[i / 8] ^ b[i / 8]) & (128U >> (i % 8))) return 0;
  return 1;
}
int
mulib_container_network_add (mulib_container_network *n, const mulib_container_network_route *r)
{
  mulib_container_network_route copy, *next;
  size_t i;
  unsigned bit, bits;
  if (!n || !r || !r->device || !r->device->queue_capacity) return MULIB_CT_EINVAL;
  if (n->busy) return MULIB_CT_EBUSY;
  bits = r->family == 4 ? 32 : r->family == 6 ? 128 : 0;
  if (!bits || r->prefix > bits) return MULIB_CT_EINVAL;
  copy = *r;
  for (bit = r->prefix; bit < 128; ++bit)
    copy.network[bit / 8] &= (unsigned char)~(128U >> (bit % 8));
  for (i = 0; i < n->count; ++i)
    if (n->routes[i].family == r->family && n->routes[i].prefix == r->prefix
        && n->routes[i].metric == r->metric && !memcmp (n->routes[i].network, copy.network, 16))
      return MULIB_CT_EEXIST;
  if (n->count >= n->max_routes) return MULIB_CT_ELIMIT;
  next = (mulib_container_network_route *)MULIB_CT_REALLOC (n->routes,
      (n->count + 1) * sizeof (*next));
  if (!next) return MULIB_CT_ENOMEM;
  n->routes = next; next[n->count++] = copy;
  return 0;
}
int mulib_container_network_remove (mulib_container_network *n, size_t i)
{
  if (!n) return MULIB_CT_EINVAL;
  if (n->busy) return MULIB_CT_EBUSY;
  if (i >= n->count) return MULIB_CT_ENOENT;
  memmove (n->routes + i, n->routes + i + 1, (--n->count - i) * sizeof (*n->routes));
  return 0;
}
int
mulib_container_network_policy (mulib_container_network *n,
    const mulib_packetrules_rule_t *rules, size_t count, int action)
{
  mulib_packetrules_rule_t *copy = NULL;
  if (!n || !mulib_packetrules_validate (rules, count) || (action != 0 && action != 1))
    return MULIB_CT_EINVAL;
  if (n->busy) return MULIB_CT_EBUSY;
  if (count > n->max_rules || count > SIZE_MAX / sizeof (*copy)) return MULIB_CT_ELIMIT;
  if (count)
    {
      copy = (mulib_packetrules_rule_t *)MULIB_CT_MALLOC (count * sizeof (*copy));
      if (!copy) return MULIB_CT_ENOMEM;
      memcpy (copy, rules, count * sizeof (*copy));
    }
  MULIB_CT_FREE (n->rules); n->rules = copy; n->rule_count = count; n->default_action = action;
  return 0;
}
const mulib_container_network_route *
mulib_container_network_lookup (const mulib_container_network *n, unsigned family,
                                 const unsigned char *address)
{
  const mulib_container_network_route *best = NULL;
  size_t i;
  if (!n || !address || (family != 4 && family != 6)) return NULL;
  for (i = 0; i < n->count; ++i)
    {
      const mulib_container_network_route *r = n->routes + i;
      if (r->family != family || !r->device->up
          || !mulib_container_network_prefix_ (r->network, address, r->prefix)) continue;
      if (!best || r->prefix > best->prefix || (r->prefix == best->prefix && r->metric < best->metric))
        best = r;
    }
  return best;
}
int
mulib_container_network_check (mulib_container_network *n, const void *p, size_t len, size_t *matched)
{
  int rc;
  if (!n) return MULIB_CT_EINVAL;
  rc = mulib_packetrules_apply (n->rules, n->rule_count, n->default_action, p, len, matched);
  if (rc < 0) return MULIB_CT_EFORMAT;
  if (!rc) { n->denied = mulib_ct_counter_add (n->denied, 1); return MULIB_CT_EACCESS; }
  n->accepted = mulib_ct_counter_add (n->accepted, 1);
  return 0;
}
int mulib_container_network_send (mulib_container_network *n, const void *p, size_t len)
{
  mulib_packetfiter_view_t packet;
  const mulib_container_network_route *r;
  mulib_ct_blob frame = { NULL, 0, 0 };
  int rc;
  if (!n) return MULIB_CT_EINVAL;
  if (n->busy) return MULIB_CT_EBUSY;
  rc = mulib_container_network_check (n, p, len, NULL);
  if (rc) return rc;
  if (mulib_packetfiter_parse (p, len, &packet) != 1) return MULIB_CT_EFORMAT;
  r = mulib_container_network_lookup (n, packet.family, packet.destination);
  if (!r) { n->unroutable = mulib_ct_counter_add (n->unroutable, 1); return MULIB_CT_ENOENT; }
  if (len > r->device->mtu) return MULIB_CT_ELIMIT;
  if (r->device->kind == MULIB_CONTAINER_NETDEV_ETHERNET)
    {
      unsigned char header[14];
      memcpy (header, r->next_hop_mac, 6); memcpy (header + 6, r->device->mac, 6);
      header[12] = packet.family == 4 ? 8 : 0x86; header[13] = packet.family == 4 ? 0 : 0xdd;
      rc = mulib_ct_blob_append (&frame, header, 14, r->device->mtu + 14);
      if (!rc) rc = mulib_ct_blob_append (&frame, p, len, r->device->mtu + 14);
      if (rc) { mulib_ct_blob_free (&frame); return rc; }
      p = frame.data; len = frame.length;
    }
  n->busy = 1; rc = mulib_container_netdev_send (r->device, p, len); n->busy = 0;
  mulib_ct_blob_free (&frame);
  return rc;
}
#endif
