/** @file mulib-ocicontainernetwork.h
 * @brief Validated IPv4 container address/prefix/gateway plans, no network
 * changes. Define MULIB_IMPL_OCICONTAINERNETWORK plus INET4 and PARSEC. */
#ifndef MULIB_OCICONTAINERNETWORK_H
#define MULIB_OCICONTAINERNETWORK_H
#include "mulib-inet4.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_ocicontainernetwork
  {
    mulib_inet4_t address, gateway;
    unsigned prefix;
    int has_gateway;
  } mulib_ocicontainernetwork_t;
  /** @brief Parse strict literals, prefix 0..32; optional gateway must be in
   * subnet and differ from address. 1 success, output unchanged on invalid
   * plan. No DHCP, DNS, interfaces, NAT or namespace operations. */
  int mulib_ocicontainernetwork_init (mulib_ocicontainernetwork_t *out,
                                      const char *address, unsigned prefix,
                                      const char *gateway);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_OCICONTAINERNETWORK)                                   \
    && !defined(MULIB_OCICONTAINERNETWORK_IMPLEMENTED)
#define MULIB_OCICONTAINERNETWORK_IMPLEMENTED
#include <string.h>
int
mulib_ocicontainernetwork_init (mulib_ocicontainernetwork_t *out,
                                const char *s, unsigned prefix, const char *g)
{
  mulib_ocicontainernetwork_t v;
  if (!out || !s || prefix > 32)
    return 0;
  memset (&v, 0, sizeof (v));
  v.prefix = prefix;
  v.has_gateway = g != NULL;
  if (!mulib_inet4_parse (s, strlen (s), &v.address))
    return 0;
  if (g
      && (!mulib_inet4_parse (g, strlen (g), &v.gateway)
          || !mulib_inet4_contains (&v.address, prefix, &v.gateway)
          || !memcmp (v.address.bytes, v.gateway.bytes, 4)))
    return 0;
  *out = v;
  return 1;
}
#endif
