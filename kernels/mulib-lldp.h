/** @file mulib-lldp.h
 * @brief Decode bounds-checked Ethernet LLDP TLVs
 * C11, single-header, define MULIB_IMPL_LLDP exactly once.
 */
#ifndef MULIB_LLDP_H
#define MULIB_LLDP_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_lldp_tlv
  {
    uint8_t type;
    const uint8_t *value;
    size_t length;
  } mulib_lldp_tlv_t;
  int mulib_lldp_foreach (const void *buf, size_t n,
                          int (*cb) (const mulib_lldp_tlv_t *, void *),
                          void *ctx);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_LLDP) && !defined(MULIB_LLDP_IMPLEMENTED)
#define MULIB_LLDP_IMPLEMENTED
#include <errno.h>
int
mulib_lldp_foreach (const void *buf, size_t n,
                    int (*cb) (const mulib_lldp_tlv_t *, void *), void *ctx)
{
  if (!buf || !cb)
    return -EINVAL;
  const uint8_t *b = buf;
  size_t i = 0;
  while (i < n)
    {
      if (n - i < 2)
        return -EBADMSG;
      unsigned h = (unsigned)b[i] << 8 | b[i + 1];
      i += 2;
      size_t len = h & 511u;
      if (n - i < len)
        return -EBADMSG;
      mulib_lldp_tlv_t tlv = { (uint8_t)(h >> 9), b + i, len };
      int r = cb (&tlv, ctx);
      if (r)
        return r;
      i += len;
      if (tlv.type == 0)
        return len == 0 ? 0 : -EBADMSG;
    }
  return -EBADMSG;
}
#endif
