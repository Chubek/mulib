/** @file mulib-deflate.h
 * @brief Bounded RFC 1951 raw DEFLATE codec. Define MULIB_IMPL_DEFLATE and
 * MULIB_IMPL_BUFFER. Decoder supports stored, fixed and dynamic blocks;
 * encoder uses fixed Huffman codes and a bounded greedy LZ77 matcher.
 */
#ifndef MULIB_DEFLATE_H
#define MULIB_DEFLATE_H
#include "mulib-buffer.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Codec results; output is unchanged on every failure. */
  enum mulib_deflate_result
  {
    MULIB_DEFLATE_OK = 0,
    MULIB_DEFLATE_EINVAL = -1,
    MULIB_DEFLATE_EFORMAT = -2,
    MULIB_DEFLATE_ELIMIT = -3,
    MULIB_DEFLATE_ENOMEM = -4
  };
  /** @brief Append a complete raw stream. Input may alias out. No zlib/gzip
   * wrapper. */
  int mulib_deflate_encode (const void *data, size_t size,
                            mulib_buffer_t *out);
  /** @brief Decode exactly one raw stream, rejecting trailing bytes. limit
   * bounds decoded bytes, independent of out's initial length. Input may alias
   * out. */
  int mulib_deflate_decode (const void *data, size_t size, size_t limit,
                            mulib_buffer_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_DEFLATE) && !defined(MULIB_DEFLATE_IMPLEMENTED)
#define MULIB_DEFLATE_IMPLEMENTED
typedef struct mulib_deflate_bits_
{
  const unsigned char *data;
  size_t size, pos;
  unsigned bits, count;
} mulib_deflate_bits_;
typedef struct mulib_deflate_tree_
{
  unsigned count[16], symbol[288];
} mulib_deflate_tree_;
static const unsigned mulib_deflate_lbase_[29]
    = { 3,  4,  5,  6,  7,  8,  9,  10, 11,  13,  15,  17,  19,  23, 27,
        31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258 };
static const unsigned mulib_deflate_lextra_[29]
    = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
        2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0 };
static const unsigned mulib_deflate_dbase_[30]
    = { 1,    2,    3,    4,    5,    7,    9,    13,    17,    25,
        33,   49,   65,   97,   129,  193,  257,  385,   513,   769,
        1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577 };
static const unsigned mulib_deflate_dextra_[30]
    = { 0, 0, 0, 0, 1, 1, 2, 2,  3,  3,  4,  4,  5,  5,  6,
        6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13 };
static int
mulib_deflate_get_ (mulib_deflate_bits_ *b, unsigned n, unsigned *v)
{
  while (b->count < n)
    {
      if (b->pos == b->size)
        return 0;
      b->bits |= (unsigned)b->data[b->pos++] << b->count;
      b->count += 8;
    }
  *v = b->bits & ((1u << n) - 1u);
  b->bits >>= n;
  b->count -= n;
  return 1;
}
/* Return unused code space; negative means oversubscribed. */
static int
mulib_deflate_tree_make_ (mulib_deflate_tree_ *t, const unsigned *lengths,
                          unsigned n)
{
  unsigned offsets[16] = { 0 }, i;
  int left = 1;
  memset (t, 0, sizeof (*t));
  for (i = 0; i < n; ++i)
    {
      if (lengths[i] > 15)
        return -1;
      ++t->count[lengths[i]];
    }
  for (i = 1; i <= 15; ++i)
    {
      left = left * 2 - (int)t->count[i];
      if (left < 0)
        return -1;
      if (i < 15)
        offsets[i + 1] = offsets[i] + t->count[i];
    }
  for (i = 0; i < n; ++i)
    if (lengths[i])
      t->symbol[offsets[lengths[i]]++] = i;
  return left;
}
static int
mulib_deflate_symbol_ (mulib_deflate_bits_ *b, const mulib_deflate_tree_ *t,
                       unsigned *s)
{
  unsigned code = 0, first = 0, index = 0, len, bit;
  for (len = 1; len <= 15; ++len)
    {
      if (!mulib_deflate_get_ (b, 1, &bit))
        return 0;
      code |= bit;
      if (code >= first && code - first < t->count[len])
        {
          *s = t->symbol[index + code - first];
          return 1;
        }
      index += t->count[len];
      first = (first + t->count[len]) << 1;
      code <<= 1;
    }
  return 0;
}
static void
mulib_deflate_fixed_ (unsigned *l, unsigned *d)
{
  unsigned i;
  for (i = 0; i < 288; ++i)
    l[i] = i < 144 ? 8 : i < 256 ? 9 : i < 280 ? 7 : 8;
  for (i = 0; i < 32; ++i)
    d[i] = 5;
}
static int
mulib_deflate_dynamic_ (mulib_deflate_bits_ *b, mulib_deflate_tree_ *lt,
                        mulib_deflate_tree_ *dt)
{
  static const unsigned order[19]
      = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };
  unsigned nl, nd, nc, i, s, extra, repeat, value, cl[19] = { 0 }, lens[320];
  mulib_deflate_tree_ ct;
  int left;
  if (!mulib_deflate_get_ (b, 5, &nl) || !mulib_deflate_get_ (b, 5, &nd)
      || !mulib_deflate_get_ (b, 4, &nc))
    return 0;
  nl += 257;
  nd += 1;
  nc += 4;
  if (nl > 286)
    return 0;
  for (i = 0; i < nc; ++i)
    if (!mulib_deflate_get_ (b, 3, &cl[order[i]]))
      return 0;
  if (mulib_deflate_tree_make_ (&ct, cl, 19) != 0)
    return 0;
  i = 0;
  while (i < nl + nd)
    {
      if (!mulib_deflate_symbol_ (b, &ct, &s))
        return 0;
      if (s < 16)
        {
          lens[i++] = s;
          continue;
        }
      if (s == 16 && !i)
        return 0;
      value = s == 16 ? lens[i - 1] : 0;
      if (!mulib_deflate_get_ (b, s == 16 ? 2 : s == 17 ? 3 : 7, &extra))
        return 0;
      repeat = extra + (s == 18 ? 11 : 3);
      if (repeat > nl + nd - i)
        return 0;
      while (repeat--)
        lens[i++] = value;
    }
  if (!lens[256])
    return 0;
  left = mulib_deflate_tree_make_ (lt, lens, nl);
  if (left < 0 || (left && (lt->count[1] != 1 || nl - lt->count[0] != 1)))
    return 0;
  left = mulib_deflate_tree_make_ (dt, lens + nl, nd);
  if (left < 0
      || (left && dt->count[0] != nd
          && (dt->count[1] != 1 || nd - dt->count[0] != 1)))
    return 0;
  return 1;
}
int
mulib_deflate_decode (const void *data, size_t size, size_t limit,
                      mulib_buffer_t *out)
{
  mulib_deflate_bits_ b;
  mulib_buffer_t tmp = { 0 };
  unsigned final = 0, type, v, w, s, len, dist, i, l[288], d[32];
  mulib_deflate_tree_ lt, dt;
  int result = MULIB_DEFLATE_EFORMAT;
  if (!out || (!data && size))
    return MULIB_DEFLATE_EINVAL;
  memset (&b, 0, sizeof (b));
  b.data = (const unsigned char *)data;
  b.size = size;
  while (!final)
    {
      if (!mulib_deflate_get_ (&b, 1, &final)
          || !mulib_deflate_get_ (&b, 2, &type))
        goto done;
      if (type == 0)
        {
          b.bits = b.count = 0;
          if (!mulib_deflate_get_ (&b, 16, &v)
              || !mulib_deflate_get_ (&b, 16, &w) || (v ^ w) != 65535
              || v > b.size - b.pos)
            goto done;
          if (v > limit - tmp.length)
            {
              result = MULIB_DEFLATE_ELIMIT;
              goto done;
            }
          if (!mulib_buffer_append (&tmp, b.data + b.pos, v))
            {
              result = MULIB_DEFLATE_ENOMEM;
              goto done;
            }
          b.pos += v;
          continue;
        }
      if (type == 3)
        goto done;
      if (type == 1)
        {
          mulib_deflate_fixed_ (l, d);
          (void)mulib_deflate_tree_make_ (&lt, l, 288);
          (void)mulib_deflate_tree_make_ (&dt, d, 32);
        }
      else if (!mulib_deflate_dynamic_ (&b, &lt, &dt))
        goto done;
      for (;;)
        {
          if (!mulib_deflate_symbol_ (&b, &lt, &s))
            goto done;
          if (s == 256)
            break;
          if (s < 256)
            {
              unsigned char c = (unsigned char)s;
              if (tmp.length == limit)
                {
                  result = MULIB_DEFLATE_ELIMIT;
                  goto done;
                }
              if (!mulib_buffer_append (&tmp, &c, 1))
                {
                  result = MULIB_DEFLATE_ENOMEM;
                  goto done;
                }
            }
          else
            {
              if (s > 285
                  || !mulib_deflate_get_ (&b, mulib_deflate_lextra_[s - 257],
                                          &v))
                goto done;
              len = mulib_deflate_lbase_[s - 257] + v;
              if (!mulib_deflate_symbol_ (&b, &dt, &s) || s >= 30
                  || !mulib_deflate_get_ (&b, mulib_deflate_dextra_[s], &v))
                goto done;
              dist = mulib_deflate_dbase_[s] + v;
              if (dist > tmp.length)
                goto done;
              if (len > limit - tmp.length)
                {
                  result = MULIB_DEFLATE_ELIMIT;
                  goto done;
                }
              for (i = 0; i < len; ++i)
                {
                  unsigned char c = tmp.data[tmp.length - dist];
                  if (!mulib_buffer_append (&tmp, &c, 1))
                    {
                      result = MULIB_DEFLATE_ENOMEM;
                      goto done;
                    }
                }
            }
        }
    }
  if (b.pos != b.size)
    goto done;
  result = mulib_buffer_append (out, tmp.data, tmp.length)
               ? MULIB_DEFLATE_OK
               : MULIB_DEFLATE_ENOMEM;
done:
  mulib_buffer_free (&tmp);
  return result;
}
typedef struct mulib_deflate_writer_
{
  mulib_buffer_t bytes;
  unsigned bits, count;
} mulib_deflate_writer_;
static int
mulib_deflate_put_ (mulib_deflate_writer_ *w, unsigned v, unsigned n)
{
  w->bits |= v << w->count;
  w->count += n;
  while (w->count >= 8)
    {
      unsigned char c = (unsigned char)w->bits;
      if (!mulib_buffer_append (&w->bytes, &c, 1))
        return 0;
      w->bits >>= 8;
      w->count -= 8;
    }
  return 1;
}
static int
mulib_deflate_code_ (mulib_deflate_writer_ *w, unsigned code, unsigned n)
{
  unsigned reversed = 0, i;
  for (i = 0; i < n; ++i)
    {
      reversed = (reversed << 1) | (code & 1);
      code >>= 1;
    }
  return mulib_deflate_put_ (w, reversed, n);
}
static int
mulib_deflate_literal_ (mulib_deflate_writer_ *w, unsigned s)
{
  return s < 144   ? mulib_deflate_code_ (w, s + 48, 8)
         : s < 256 ? mulib_deflate_code_ (w, s - 144 + 400, 9)
         : s < 280 ? mulib_deflate_code_ (w, s - 256, 7)
                   : mulib_deflate_code_ (w, s - 280 + 192, 8);
}
int
mulib_deflate_encode (const void *data, size_t size, mulib_buffer_t *out)
{
  const unsigned char *p = (const unsigned char *)data;
  size_t *last, pos = 0, prev, dist, len, k;
  unsigned hash, lc, dc;
  mulib_deflate_writer_ w;
  int result = MULIB_DEFLATE_ENOMEM;
  if (!out || (!data && size))
    return MULIB_DEFLATE_EINVAL;
  memset (&w, 0, sizeof (w));
  last = (size_t *)malloc (4096 * sizeof (*last));
  if (!last)
    return result;
  for (k = 0; k < 4096; ++k)
    last[k] = SIZE_MAX;
  if (!mulib_deflate_put_ (&w, 3, 3))
    goto done;
  while (pos < size)
    {
      len = 0;
      dist = 0;
      if (size - pos >= 3)
        {
          hash = ((unsigned)p[pos] * 251u + (unsigned)p[pos + 1] * 31u
                  + p[pos + 2])
                 & 4095u;
          prev = last[hash];
          last[hash] = pos;
          if (prev != SIZE_MAX && pos - prev <= 32768)
            {
              while (len < 258 && len < size - pos
                     && p[prev + len] == p[pos + len])
                ++len;
              dist = pos - prev;
            }
        }
      if (len < 3)
        {
          if (!mulib_deflate_literal_ (&w, p[pos++]))
            goto done;
          continue;
        }
      for (lc = 0; lc < 28 && mulib_deflate_lbase_[lc + 1] <= len; ++lc)
        {
        }
      for (dc = 0; dc < 29 && mulib_deflate_dbase_[dc + 1] <= dist; ++dc)
        {
        }
      if (!mulib_deflate_literal_ (&w, 257 + lc)
          || !mulib_deflate_put_ (&w, (unsigned)len - mulib_deflate_lbase_[lc],
                                  mulib_deflate_lextra_[lc])
          || !mulib_deflate_code_ (&w, dc, 5)
          || !mulib_deflate_put_ (&w,
                                  (unsigned)dist - mulib_deflate_dbase_[dc],
                                  mulib_deflate_dextra_[dc]))
        goto done;
      pos += len;
    }
  if (!mulib_deflate_literal_ (&w, 256))
    goto done;
  if (w.count && !mulib_deflate_put_ (&w, 0, 8 - w.count))
    goto done;
  result = mulib_buffer_append (out, w.bytes.data, w.bytes.length)
               ? MULIB_DEFLATE_OK
               : MULIB_DEFLATE_ENOMEM;
done:
  free (last);
  mulib_buffer_free (&w.bytes);
  return result;
}
#endif
