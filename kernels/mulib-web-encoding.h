/* MuLIB browser staging addition: web-encoding
 * Implemented subset: UTF-8 validation/replacement and Windows-1252 decoding.
 * Remaining: Streaming state, BOM handling, fatal mode, UTF-16 and other
 * legacy encoding tables.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEB_ENCODING_H
#define MULIB_WEB_ENCODING_H
#include "detail/mulib-browser-additions-common.h"

/* One replacement for a malformed or truncated prefix. A byte outside the
 * current continuation range stays in the input for the next iteration. */
static inline uint32_t
mulib_web_encoding_utf8_ (muba_span s, size_t *i)
{
  unsigned lead = s.data[(*i)++], remaining, lower = 0x80, upper = 0xbf;
  uint32_t c;
  if (lead < 0x80)
    return lead;
  if (lead >= 0xc2 && lead <= 0xdf)
    {
      c = lead & 0x1f;
      remaining = 1;
    }
  else if (lead >= 0xe0 && lead <= 0xef)
    {
      c = lead & 0x0f;
      remaining = 2;
      if (lead == 0xe0)
        lower = 0xa0;
      else if (lead == 0xed)
        upper = 0x9f;
    }
  else if (lead >= 0xf0 && lead <= 0xf4)
    {
      c = lead & 0x07;
      remaining = 3;
      if (lead == 0xf0)
        lower = 0x90;
      else if (lead == 0xf4)
        upper = 0x8f;
    }
  else
    return 0xfffd;

  while (remaining)
    {
      if (*i == s.len)
        return 0xfffd;
      unsigned byte = s.data[*i];
      if (byte < lower || byte > upper)
        return 0xfffd;
      ++*i;
      --remaining;
      c = (c << 6) | (byte & 0x3f);
      lower = 0x80;
      upper = 0xbf;
    }
  return c;
}

/* One-shot decode without removing a BOM. Replace out only on success. */
static inline int
mulib_web_encoding_decode (muba_span s, int cp1252, muba_buf *out)
{
  static const uint16_t map[32] = {
    0x20ac, 0x81,   0x201a, 0x192,  0x201e, 0x2026, 0x2020, 0x2021,
    0x2c6,  0x2030, 0x160,  0x2039, 0x152,  0x8d,   0x17d,  0x8f,
    0x90,   0x2018, 0x2019, 0x201c, 0x201d, 0x2022, 0x2013, 0x2014,
    0x2dc,  0x2122, 0x161,  0x203a, 0x153,  0x9d,   0x17e,  0x178
  };
  if (!out || !muba_span_ok (s))
    return 0;
  muba_buf b = { 0 };
  size_t i = 0;
  while (i < s.len)
    {
      uint32_t c;
      if (cp1252)
        {
          c = s.data[i++];
          if (c >= 128 && c < 160)
            c = map[c - 128];
        }
      else
        c = mulib_web_encoding_utf8_ (s, &i);
      if (!muba_utf8_put (&b, c, 0))
        {
          muba_free (&b);
          return 0;
        }
    }
  return muba_commit (out, &b);
}
#endif /* MULIB_WEB_ENCODING_H */
