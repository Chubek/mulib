/** @file mulib-packetrules.h
 * @brief Ordered IP-prefix, protocol and port-range packet rules.
 * Define MULIB_IMPL_PACKETRULES and MULIB_IMPL_PACKETFITER once.
 */
#ifndef MULIB_PACKETRULES_H
#define MULIB_PACKETRULES_H
#include "mulib-packetfiter.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_packetrules_rule
  {
    unsigned family, protocol; /* family 4/6, protocol 0 wildcard */
    unsigned char source[16], destination[16];
    unsigned source_prefix, destination_prefix;
    uint16_t source_low, source_high, destination_low, destination_high;
    int ports, action; /* ports 0 wildcard/1 ranges; action 0 drop/1 accept */
  } mulib_packetrules_rule_t;
  /** @brief Validate all rules, <=65536 entries; 1 valid, 0 invalid. */
  int mulib_packetrules_validate (const mulib_packetrules_rule_t *, size_t);
  /** @brief First matching rule wins, else default_action (0/1). Returns -1
   * for invalid rules, -2 for invalid/unsupported packet, or action 0/1.
   * matched is SIZE_MAX for no match/error. Port rules never match fragments.
   */
  int mulib_packetrules_apply (const mulib_packetrules_rule_t *, size_t,
                               int default_action, const void *, size_t,
                               size_t *matched);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_PACKETRULES) && !defined(MULIB_PACKETRULES_IMPLEMENTED)
#define MULIB_PACKETRULES_IMPLEMENTED
int
mulib_packetrules_validate (const mulib_packetrules_rule_t *r, size_t n)
{
  if ((!r && n) || n > 65536)
    return 0;
  for (size_t i = 0; i < n; ++i)
    {
      unsigned bits = r[i].family == 4 ? 32 : r[i].family == 6 ? 128 : 0;
      if (!bits || r[i].protocol > 255 || r[i].source_prefix > bits
          || r[i].destination_prefix > bits
          || (r[i].ports != 0 && r[i].ports != 1)
          || (r[i].action != 0 && r[i].action != 1)
          || (r[i].ports
              && (r[i].source_low > r[i].source_high
                  || r[i].destination_low > r[i].destination_high)))
        return 0;
    }
  return 1;
}
static int
mulib_packetrules_prefix_ (const unsigned char *a, const unsigned char *b,
                           unsigned bits)
{
  for (unsigned i = 0; i < bits; ++i)
    if ((a[i / 8] ^ b[i / 8]) & (128u >> (i % 8)))
      return 0;
  return 1;
}
int
mulib_packetrules_apply (const mulib_packetrules_rule_t *r, size_t n,
                         int fallback, const void *p, size_t len,
                         size_t *matched)
{
  mulib_packetfiter_view_t v;
  if (matched)
    *matched = SIZE_MAX;
  if ((fallback != 0 && fallback != 1) || !mulib_packetrules_validate (r, n))
    return -1;
  if (mulib_packetfiter_parse (p, len, &v) != 1)
    return -2;
  for (size_t i = 0; i < n; ++i)
    {
      if (r[i].family != v.family
          || (r[i].protocol && r[i].protocol != v.protocol)
          || !mulib_packetrules_prefix_ (r[i].source, v.source,
                                         r[i].source_prefix)
          || !mulib_packetrules_prefix_ (r[i].destination, v.destination,
                                         r[i].destination_prefix))
        continue;
      if (r[i].ports
          && (!v.has_ports || v.source_port < r[i].source_low
              || v.source_port > r[i].source_high
              || v.destination_port < r[i].destination_low
              || v.destination_port > r[i].destination_high))
        continue;
      if (matched)
        *matched = i;
      return r[i].action;
    }
  return fallback;
}
#endif
