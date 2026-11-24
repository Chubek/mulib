/** @file mulib-mobi.h
 * @brief PalmDB/PalmDOC and MOBI 6/7/8 inspection and bounded text decoding.
 * Define MULIB_IMPL_MOBI and MULIB_IMPL_BUFFER. All reader views borrow input.
 */
#ifndef MULIB_MOBI_H
#define MULIB_MOBI_H
#include "mulib-buffer.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Zero success, positive absence, negative errors. */
  enum mulib_mobi_result
  {
    MULIB_MOBI_OK = 0,
    MULIB_MOBI_NOTFOUND = 1,
    MULIB_MOBI_EINVAL = -1,
    MULIB_MOBI_EFORMAT = -2,
    MULIB_MOBI_EUNSUPPORTED = -3,
    MULIB_MOBI_EENCRYPTED = -4,
    MULIB_MOBI_ELIMIT = -5,
    MULIB_MOBI_ENOMEM = -6
  };
  /** @brief Borrowed binary span, never implicitly NUL terminated. */
  typedef struct mulib_mobi_span
  {
    const unsigned char *data;
    size_t size;
  } mulib_mobi_span_t;
  /** @brief Immutable borrowed view. No cleanup required; fields are
   * read-only. */
  typedef struct mulib_mobi_reader
  {
    const unsigned char *data;
    size_t size, record_count, header_record, text_records;
    uint32_t text_length, version, encoding, first_resource;
    unsigned compression, extra_flags, record_size;
    mulib_mobi_span_t title, exth;
  } mulib_mobi_reader_t;
  /** @brief Open a PalmDOC or BOOKMOBI database at record zero. No
   * allocations; malformed structure, unsupported compression/versions and
   * encryption fail. Output changes only on success. */
  int mulib_mobi_open (mulib_mobi_reader_t *reader, const void *data,
                       size_t size);
  /** @brief Select another MOBI header in an already validated database (KF8
   * hybrid support). Output may equal database; unchanged on failure. */
  int mulib_mobi_select (const mulib_mobi_reader_t *database, size_t record,
                         mulib_mobi_reader_t *out);
  /** @brief Borrow an absolute database record. Output unchanged on failure.
   */
  int mulib_mobi_record (const mulib_mobi_reader_t *reader, size_t index,
                         mulib_mobi_span_t *out);
  /** @brief Borrow the occurrence-th EXTH value of type (zero-based). Returns
   * NOTFOUND when absent; supports duplicate metadata fields. */
  int mulib_mobi_metadata (const mulib_mobi_reader_t *reader, uint32_t type,
                           size_t occurrence, mulib_mobi_span_t *out);
  /** @brief Decode one PalmDOC compressed record, with local backreferences
   * and a hard output limit. Append is failure-atomic; input may alias out. */
  int mulib_mobi_palmdoc_decode (const void *data, size_t size, size_t limit,
                                 mulib_buffer_t *out);
  /** @brief Append all decoded text records, stripping declared trailing data.
   * Returns original encoding/markup bytes, not rendered text. limit bounds
   * total decoded length; exact header text length is required.
   * Failure-atomic. */
  int mulib_mobi_text (const mulib_mobi_reader_t *reader, size_t limit,
                       mulib_buffer_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_MOBI) && !defined(MULIB_MOBI_IMPLEMENTED)
#define MULIB_MOBI_IMPLEMENTED
static unsigned
mulib_mobi_be16_ (const unsigned char *p)
{
  return ((unsigned)p[0] << 8) | p[1];
}
static uint32_t
mulib_mobi_be32_ (const unsigned char *p)
{
  return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16)
         | ((uint32_t)p[2] << 8) | p[3];
}
int
mulib_mobi_record (const mulib_mobi_reader_t *r, size_t index,
                   mulib_mobi_span_t *out)
{
  size_t start, end;
  if (!r || !r->data || !out || index >= r->record_count)
    return MULIB_MOBI_EINVAL;
  start = mulib_mobi_be32_ (r->data + 78 + index * 8);
  end = index + 1 < r->record_count
            ? mulib_mobi_be32_ (r->data + 78 + (index + 1) * 8)
            : r->size;
  if (start > end || end > r->size)
    return MULIB_MOBI_EFORMAT;
  out->data = r->data + start;
  out->size = end - start;
  return 0;
}
int
mulib_mobi_select (const mulib_mobi_reader_t *db, size_t record,
                   mulib_mobi_reader_t *out)
{
  mulib_mobi_reader_t r;
  mulib_mobi_span_t s;
  const unsigned char *p;
  size_t hlen, offset, length, i, pos, count;
  int rc;
  if (!db || !out)
    return MULIB_MOBI_EINVAL;
  rc = mulib_mobi_record (db, record, &s);
  if (rc)
    return rc;
  if (s.size < 16)
    return MULIB_MOBI_EFORMAT;
  r = *db;
  r.header_record = record;
  r.version = 0;
  r.encoding = 1252;
  r.extra_flags = 0;
  memset (&r.title, 0, sizeof (r.title));
  memset (&r.exth, 0, sizeof (r.exth));
  p = s.data;
  r.compression = mulib_mobi_be16_ (p);
  r.text_length = mulib_mobi_be32_ (p + 4);
  r.text_records = mulib_mobi_be16_ (p + 8);
  r.record_size = mulib_mobi_be16_ (p + 10);
  if (r.text_records > r.record_count - record - 1
      || (!r.text_records && r.text_length)
      || (r.text_records && !r.record_size)
      || (uint64_t)r.text_length > (uint64_t)r.text_records * r.record_size)
    return MULIB_MOBI_EFORMAT;
  if (r.compression != 1 && r.compression != 2)
    return MULIB_MOBI_EUNSUPPORTED;
  r.first_resource = (uint32_t)(record + r.text_records + 1);
  if (!memcmp (r.data + 60, "TEXtREAd", 8))
    {
      if (record)
        return MULIB_MOBI_EUNSUPPORTED;
      r.title.data = r.data;
      r.title.size = 0;
      while (r.title.size < 32 && r.data[r.title.size])
        ++r.title.size;
      *out = r;
      return 0;
    }
  if (s.size < 132 || memcmp (p + 16, "MOBI", 4))
    return MULIB_MOBI_EFORMAT;
  hlen = mulib_mobi_be32_ (p + 20);
  if (hlen < 116 || hlen > s.size - 16)
    return MULIB_MOBI_EFORMAT;
  if (mulib_mobi_be16_ (p + 12))
    return MULIB_MOBI_EENCRYPTED;
  if (mulib_mobi_be32_ (p + 24) != 2)
    return MULIB_MOBI_EUNSUPPORTED;
  r.encoding = mulib_mobi_be32_ (p + 28);
  r.version = mulib_mobi_be32_ (p + 36);
  if ((r.version != 6 && r.version != 7 && r.version != 8)
      || (r.encoding != 1252 && r.encoding != 65001))
    return MULIB_MOBI_EUNSUPPORTED;
  offset = mulib_mobi_be32_ (p + 84);
  length = mulib_mobi_be32_ (p + 88);
  if (offset > s.size || length > s.size - offset
      || (length && offset < 16 + hlen))
    return MULIB_MOBI_EFORMAT;
  r.title.data = p + offset;
  r.title.size = length;
  offset = mulib_mobi_be32_ (p + 108);
  if (offset != UINT32_MAX)
    {
      if (offset > r.record_count - record || offset <= r.text_records)
        return MULIB_MOBI_EFORMAT;
      r.first_resource = (uint32_t)(record + offset);
    }
  if (hlen >= 228 && mulib_mobi_be32_ (p + 104) >= 5)
    r.extra_flags = mulib_mobi_be16_ (p + 242);
  if (mulib_mobi_be32_ (p + 128) & 64)
    {
      offset = 16 + hlen;
      if (s.size - offset < 12 || memcmp (p + offset, "EXTH", 4))
        return MULIB_MOBI_EFORMAT;
      length = mulib_mobi_be32_ (p + offset + 4);
      count = mulib_mobi_be32_ (p + offset + 8);
      if (length < 12 || length > s.size - offset || count > (length - 12) / 8)
        return MULIB_MOBI_EFORMAT;
      pos = 12;
      for (i = 0; i < count; ++i)
        {
          size_t n;
          if (length - pos < 8)
            return MULIB_MOBI_EFORMAT;
          n = mulib_mobi_be32_ (p + offset + pos + 4);
          if (n < 8 || n > length - pos)
            return MULIB_MOBI_EFORMAT;
          pos += n;
        }
      /* EXTH may include up to three zero alignment bytes. */
      if (length - pos > 3)
        return MULIB_MOBI_EFORMAT;
      while (pos < length)
        if (p[offset + pos++])
          return MULIB_MOBI_EFORMAT;
      r.exth.data = p + offset;
      r.exth.size = length;
      if (r.title.size && (size_t)(r.title.data - p) < offset + length)
        return MULIB_MOBI_EFORMAT;
    }
  *out = r;
  return 0;
}
int
mulib_mobi_open (mulib_mobi_reader_t *out, const void *data, size_t size)
{
  const unsigned char *p = (const unsigned char *)data;
  mulib_mobi_reader_t r;
  size_t count, i, previous, off;
  if (!out || (!data && size))
    return MULIB_MOBI_EINVAL;
  if (size < 78)
    return MULIB_MOBI_EFORMAT;
  if (size > UINT32_MAX)
    return MULIB_MOBI_ELIMIT;
  if ((memcmp (p + 60, "BOOKMOBI", 8) && memcmp (p + 60, "TEXtREAd", 8))
      || (mulib_mobi_be16_ (p + 32) & 1) || mulib_mobi_be32_ (p + 72))
    return MULIB_MOBI_EUNSUPPORTED;
  count = mulib_mobi_be16_ (p + 76);
  if (!count || count > (size - 78) / 8)
    return MULIB_MOBI_EFORMAT;
  previous = 78 + count * 8;
  for (i = 0; i < count; ++i)
    {
      off = mulib_mobi_be32_ (p + 78 + i * 8);
      if (off < previous || off > size)
        return MULIB_MOBI_EFORMAT;
      previous = off;
    }
  memset (&r, 0, sizeof (r));
  r.data = p;
  r.size = size;
  r.record_count = count;
  return mulib_mobi_select (&r, 0, out);
}
int
mulib_mobi_metadata (const mulib_mobi_reader_t *r, uint32_t type,
                     size_t occurrence, mulib_mobi_span_t *out)
{
  size_t i, pos = 12, count;
  if (!r || !r->data || !out)
    return MULIB_MOBI_EINVAL;
  if (!r->exth.data)
    return MULIB_MOBI_NOTFOUND;
  count = mulib_mobi_be32_ (r->exth.data + 8);
  for (i = 0; i < count; ++i)
    {
      const unsigned char *p = r->exth.data + pos;
      size_t n = mulib_mobi_be32_ (p + 4);
      if (mulib_mobi_be32_ (p) == type && occurrence-- == 0)
        {
          out->data = p + 8;
          out->size = n - 8;
          return 0;
        }
      pos += n;
    }
  return MULIB_MOBI_NOTFOUND;
}
int
mulib_mobi_palmdoc_decode (const void *data, size_t size, size_t limit,
                           mulib_buffer_t *out)
{
  const unsigned char *p = (const unsigned char *)data;
  mulib_buffer_t tmp = { 0 };
  size_t pos = 0, n, dist, i;
  unsigned c, word;
  int result = MULIB_MOBI_EFORMAT;
  if (!out || (!data && size))
    return MULIB_MOBI_EINVAL;
  while (pos < size)
    {
      unsigned char pair[2];
      c = p[pos++];
      if (c >= 1 && c <= 8)
        {
          n = c;
          if (n > size - pos)
            goto done;
          if (n > limit - tmp.length)
            {
              result = MULIB_MOBI_ELIMIT;
              goto done;
            }
          if (!mulib_buffer_append (&tmp, p + pos, n))
            {
              result = MULIB_MOBI_ENOMEM;
              goto done;
            }
          pos += n;
        }
      else if (c >= 128 && c < 192)
        {
          if (pos == size)
            goto done;
          word = (c << 8) | p[pos++];
          dist = (word >> 3) & 2047;
          n = (word & 7) + 3;
          if (!dist || dist > tmp.length)
            goto done;
          if (n > limit - tmp.length)
            {
              result = MULIB_MOBI_ELIMIT;
              goto done;
            }
          for (i = 0; i < n; ++i)
            {
              unsigned char b = tmp.data[tmp.length - dist];
              if (!mulib_buffer_append (&tmp, &b, 1))
                {
                  result = MULIB_MOBI_ENOMEM;
                  goto done;
                }
            }
        }
      else
        {
          n = c >= 192 ? 2 : 1;
          pair[0] = (unsigned char)(n == 2 ? 32 : c);
          pair[1] = (unsigned char)(c ^ 128);
          if (n > limit - tmp.length)
            {
              result = MULIB_MOBI_ELIMIT;
              goto done;
            }
          if (!mulib_buffer_append (&tmp, pair, n))
            {
              result = MULIB_MOBI_ENOMEM;
              goto done;
            }
        }
    }
  result = mulib_buffer_append (out, tmp.data, tmp.length) ? 0
                                                           : MULIB_MOBI_ENOMEM;
done:
  mulib_buffer_free (&tmp);
  return result;
}
static int
mulib_mobi_trim_ (mulib_mobi_span_t *s, unsigned flags)
{
  unsigned f;
  for (f = flags >> 1; f; f >>= 1)
    if (f & 1)
      {
        size_t value = 0, used = 0;
        unsigned c = 0;
        do
          {
            if (used == s->size || used == 4)
              return 0;
            c = s->data[s->size - 1 - used];
            value |= (size_t)(c & 127) << (7 * used);
            ++used;
          }
        while (!(c & 128));
        if (value < used || value > s->size)
          return 0;
        s->size -= value;
      }
  if (flags & 1)
    {
      size_t n;
      if (!s->size)
        return 0;
      n = (s->data[s->size - 1] & 3) + 1;
      if (n > s->size)
        return 0;
      s->size -= n;
    }
  return 1;
}
int
mulib_mobi_text (const mulib_mobi_reader_t *r, size_t limit,
                 mulib_buffer_t *out)
{
  mulib_buffer_t tmp = { 0 };
  mulib_mobi_span_t s;
  size_t i;
  int rc = 0;
  if (!r || !r->data || !out)
    return MULIB_MOBI_EINVAL;
  if (r->text_length > limit)
    return MULIB_MOBI_ELIMIT;
  for (i = 0; i < r->text_records; ++i)
    {
      size_t remaining = r->text_length - tmp.length;
      size_t record_limit
          = remaining < r->record_size ? remaining : r->record_size;
      rc = mulib_mobi_record (r, r->header_record + 1 + i, &s);
      if (rc)
        break;
      if (!mulib_mobi_trim_ (&s, r->extra_flags))
        {
          rc = MULIB_MOBI_EFORMAT;
          break;
        }
      if (r->compression == 2)
        rc = mulib_mobi_palmdoc_decode (s.data, s.size, record_limit, &tmp);
      else if (s.size > record_limit)
        rc = MULIB_MOBI_EFORMAT;
      else if (!mulib_buffer_append (&tmp, s.data, s.size))
        rc = MULIB_MOBI_ENOMEM;
      if (rc == MULIB_MOBI_ELIMIT)
        rc = MULIB_MOBI_EFORMAT;
      if (rc)
        break;
    }
  if (!rc && tmp.length != r->text_length)
    rc = MULIB_MOBI_EFORMAT;
  if (!rc && !mulib_buffer_append (out, tmp.data, tmp.length))
    rc = MULIB_MOBI_ENOMEM;
  mulib_buffer_free (&tmp);
  return rc;
}
#endif
