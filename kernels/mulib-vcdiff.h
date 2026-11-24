/** @file mulib-vcdiff.h
 * @brief RFC 3284 default-table VCDIFF decoding and prefix-delta encoding.
 * Define MULIB_IMPL_VCDIFF once. No secondary compression, custom code tables,
 * application headers or nonstandard checksums; these return unsupported.
 */
#ifndef MULIB_VCDIFF_H
#define MULIB_VCDIFF_H
#include <stddef.h>
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Decode all windows using source and earlier target segments, all
   * default-table instructions/address modes. 1 success, 0 malformed, -1
   * unsupported, -2 capacity. used=0 on failure, output may be partially
   * written. Output must not overlap source or delta; all storage is
   * caller-owned. */
  int mulib_vcdiff_decode (const void *source, size_t source_length,
                           const void *delta, size_t delta_length,
                           void *output, size_t capacity, size_t *used);
  /** @brief Encode one window with common-prefix COPY and remaining ADD.
   * 1 success, 0 invalid/insufficient capacity; output unchanged on failure.
   * At most target_length+128 bytes required (if that sum is representable).
   */
  int mulib_vcdiff_encode (const void *source, size_t source_length,
                           const void *target, size_t target_length,
                           void *output, size_t capacity, size_t *used);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_VCDIFF) && !defined(MULIB_VCDIFF_IMPLEMENTED)
#define MULIB_VCDIFF_IMPLEMENTED
#include <stdint.h>
#include <string.h>
static int
mulib_vcdiff_int_ (const unsigned char *p, size_t end, size_t *pos,
                   size_t *out)
{
  size_t v = 0, steps = 0;
  while (*pos < end && steps++ < (sizeof (size_t) * 8 + 6) / 7)
    {
      unsigned b = p[(*pos)++];
      if (v > (SIZE_MAX - (b & 127)) / 128)
        return 0;
      v = v * 128 + (b & 127);
      if (!(b & 128))
        {
          *out = v;
          return 1;
        }
    }
  return 0;
}
static size_t
mulib_vcdiff_put_ (unsigned char *p, size_t v)
{
  unsigned char tmp[(sizeof (size_t) * 8 + 6) / 7];
  size_t n = 0;
  do
    {
      tmp[n++] = (unsigned char)(v & 127);
      v >>= 7;
    }
  while (v);
  for (size_t i = 0; i < n; ++i)
    p[i] = (unsigned char)(tmp[n - i - 1] | (i + 1 < n ? 128 : 0));
  return n;
}
int
mulib_vcdiff_encode (const void *source, size_t sn, const void *target,
                     size_t tn, void *output, size_t cap, size_t *used)
{
  const unsigned char *s = source, *t = target;
  unsigned char *out = output;
  unsigned char inst[32], header[128], meta[64];
  size_t prefix = 0, il = 0, hl = 5, ml = 0, dl, delta;
  if (used)
    *used = 0;
  if ((!s && sn) || (!t && tn) || !out || !used || tn > SIZE_MAX - 128)
    return 0;
  while (prefix < sn && prefix < tn && s[prefix] == t[prefix])
    ++prefix;
  if (prefix < 4)
    prefix = 0;
  if (prefix)
    {
      inst[il++] = 19;
      il += mulib_vcdiff_put_ (inst + il, prefix);
    }
  dl = tn - prefix;
  if (dl)
    {
      inst[il++] = 1;
      il += mulib_vcdiff_put_ (inst + il, dl);
    }
  ml += mulib_vcdiff_put_ (meta + ml, tn);
  meta[ml++] = 0;
  ml += mulib_vcdiff_put_ (meta + ml, dl);
  ml += mulib_vcdiff_put_ (meta + ml, il);
  ml += mulib_vcdiff_put_ (meta + ml, prefix ? 1 : 0);
  delta = ml + dl + il + (prefix ? 1 : 0);
  memcpy (header, "\xd6\xc3\xc4\0\0", 5);
  header[hl++] = prefix ? 1 : 0;
  if (prefix)
    {
      hl += mulib_vcdiff_put_ (header + hl, prefix);
      header[hl++] = 0;
    }
  hl += mulib_vcdiff_put_ (header + hl, delta);
  if (cap < hl || delta > cap - hl)
    return 0;
  memcpy (out, header, hl);
  memcpy (out + hl, meta, ml);
  hl += ml;
  if (dl)
    memcpy (out + hl, t + prefix, dl);
  hl += dl;
  memcpy (out + hl, inst, il);
  hl += il;
  if (prefix)
    out[hl++] = 0;
  *used = hl;
  return 1;
}
static void
mulib_vcdiff_code_ (unsigned c, unsigned type[2], size_t size[2],
                    unsigned mode[2])
{
  type[0] = type[1] = mode[0] = mode[1] = 0;
  size[0] = size[1] = 0;
  if (!c)
    type[0] = 2;
  else if (c <= 18)
    {
      type[0] = 1;
      size[0] = c - 1;
    }
  else if (c <= 162)
    {
      c -= 19;
      type[0] = 3;
      mode[0] = c / 16;
      size[0] = c % 16 ? c % 16 + 3 : 0;
    }
  else if (c <= 234)
    {
      c -= 163;
      type[0] = 1;
      type[1] = 3;
      size[0] = (c % 12) / 3 + 1;
      size[1] = c % 3 + 4;
      mode[1] = c / 12;
    }
  else if (c <= 246)
    {
      c -= 235;
      type[0] = 1;
      type[1] = 3;
      size[0] = c % 4 + 1;
      size[1] = 4;
      mode[1] = c / 4 + 6;
    }
  else
    {
      type[0] = 3;
      type[1] = 1;
      size[0] = 4;
      size[1] = 1;
      mode[0] = c - 247;
    }
}
int
mulib_vcdiff_decode (const void *source, size_t sn, const void *delta,
                     size_t dn, void *output, size_t cap, size_t *used)
{
  const unsigned char *s = source, *p = delta;
  unsigned char *out = output;
  size_t pos = 5, total = 0;
  if (used)
    *used = 0;
  if ((!s && sn) || !p || (!out && cap) || !used || dn < 5
      || memcmp (p, "\xd6\xc3\xc4", 3))
    return 0;
  if (p[3] || p[4])
    return -1;
  while (pos < dn)
    {
      unsigned flags = p[pos++];
      size_t sl = 0, sp = 0, de, dl, il, al, tn, ip, ap, dp, end, made = 0;
      size_t near[4] = { 0 }, same[768] = { 0 };
      unsigned slot = 0;
      const unsigned char *base = s;
      if (flags > 2)
        return -1;
      if (flags)
        {
          if (!mulib_vcdiff_int_ (p, dn, &pos, &sl)
              || !mulib_vcdiff_int_ (p, dn, &pos, &sp))
            return 0;
          if (flags == 2)
            {
              base = out;
              if (sp > total || sl > total - sp)
                return 0;
            }
          else if (sp > sn || sl > sn - sp)
            return 0;
          if (sl)
            base += sp;
        }
      if (!mulib_vcdiff_int_ (p, dn, &pos, &de) || de > dn - pos)
        return 0;
      end = pos + de;
      if (!mulib_vcdiff_int_ (p, end, &pos, &tn) || pos == end)
        return 0;
      if (p[pos++])
        return -1;
      if (tn > cap - total)
        return -2;
      if (sl > SIZE_MAX - tn)
        return 0;
      if (!mulib_vcdiff_int_ (p, end, &pos, &dl)
          || !mulib_vcdiff_int_ (p, end, &pos, &il)
          || !mulib_vcdiff_int_ (p, end, &pos, &al) || dl > end - pos
          || il > end - pos - dl || al != end - pos - dl - il)
        return 0;
      dp = pos;
      ip = dp + dl;
      ap = ip + il;
      {
        size_t dend = ip, iend = ap;
        while (ip < iend)
          {
            unsigned type[2], mode[2];
            size_t size[2];
            mulib_vcdiff_code_ (p[ip++], type, size, mode);
            for (unsigned j = 0; j < 2; ++j)
              {
                size_t k = size[j], addr, v;
                if (!type[j])
                  continue;
                if (!k && !mulib_vcdiff_int_ (p, iend, &ip, &k))
                  return 0;
                if (!k || k > tn - made)
                  return 0;
                if (type[j] == 1)
                  {
                    if (k > dend - dp)
                      return 0;
                    memcpy (out + total + made, p + dp, k);
                    dp += k;
                  }
                else if (type[j] == 2)
                  {
                    if (dp == dend)
                      return 0;
                    memset (out + total + made, p[dp++], k);
                  }
                else
                  {
                    if (mode[j] >= 6)
                      {
                        if (ap == end)
                          return 0;
                        addr = same[(mode[j] - 6) * 256 + p[ap++]];
                      }
                    else
                      {
                        if (!mulib_vcdiff_int_ (p, end, &ap, &v))
                          return 0;
                        if (!mode[j])
                          addr = v;
                        else if (mode[j] == 1)
                          {
                            if (v > sl + made)
                              return 0;
                            addr = sl + made - v;
                          }
                        else
                          {
                            if (v > SIZE_MAX - near[mode[j] - 2])
                              return 0;
                            addr = near[mode[j] - 2] + v;
                          }
                      }
                    if (addr >= sl + made)
                      return 0;
                    near[slot] = addr;
                    slot = (slot + 1) % 4;
                    same[addr % 768] = addr;
                    if (addr < sl)
                      {
                        if (k > sl - addr)
                          return 0;
                        memcpy (out + total + made, base + addr, k);
                      }
                    else
                      {
                        addr -= sl;
                        for (size_t x = 0; x < k; ++x)
                          out[total + made + x] = out[total + addr + x];
                      }
                  }
                made += k;
              }
          }
        if (made != tn || dp != dend || ap != end)
          return 0;
      }
      total += tn;
      pos = end;
    }
  *used = total;
  return 1;
}
#endif
