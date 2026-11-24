/** @file mulib-azw3.h
 * @brief Unencrypted KF8/AZW3 section selection and checked FDST flow access.
 * Define MULIB_IMPL_AZW3, MULIB_IMPL_MOBI and MULIB_IMPL_BUFFER.
 */
#ifndef MULIB_AZW3_H
#define MULIB_AZW3_H
#include "mulib-mobi.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Borrowed KF8 view, no cleanup required. Results use MULIB_MOBI_*.
   * Flow bytes are raw KF8 markup/CSS, not reconstructed XHTML chapters. */
  typedef struct mulib_azw3_reader
  {
    mulib_mobi_reader_t mobi;
    mulib_mobi_span_t fdst;
    size_t flow_count;
  } mulib_azw3_reader_t;
  /** @brief Open standalone version 8 or a version 6/7 hybrid with EXTH 121
   * and BOUNDARY record. Validate all FDST ranges. Output unchanged on
   * failure. */
  int mulib_azw3_open (mulib_azw3_reader_t *reader, const void *data,
                       size_t size);
  /** @brief Append concatenated raw text flows atomically, within limit. */
  int mulib_azw3_text (const mulib_azw3_reader_t *reader, size_t limit,
                       mulib_buffer_t *out);
  /** @brief Return a flow's byte offset/length in the decoded text. */
  int mulib_azw3_flow_range (const mulib_azw3_reader_t *reader, size_t index,
                             size_t *offset, size_t *length);
  /** @brief Decode and append one flow. limit bounds total decoded text,
   * including other flows, to bound work/memory. Failure preserves out. */
  int mulib_azw3_flow (const mulib_azw3_reader_t *reader, size_t index,
                       size_t limit, mulib_buffer_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_AZW3) && !defined(MULIB_AZW3_IMPLEMENTED)
#define MULIB_AZW3_IMPLEMENTED
static uint32_t
mulib_azw3_be32_ (const unsigned char *p)
{
  return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16)
         | ((uint32_t)p[2] << 8) | p[3];
}
int
mulib_azw3_open (mulib_azw3_reader_t *out, const void *data, size_t size)
{
  mulib_azw3_reader_t r;
  mulib_mobi_span_t s, header;
  size_t boundary, index, count, i, previous = 0;
  int rc;
  if (!out)
    return MULIB_MOBI_EINVAL;
  memset (&r, 0, sizeof (r));
  rc = mulib_mobi_open (&r.mobi, data, size);
  if (rc)
    return rc;
  if (r.mobi.version != 8)
    {
      rc = mulib_mobi_metadata (&r.mobi, 121, 0, &s);
      if (rc == MULIB_MOBI_NOTFOUND)
        return MULIB_MOBI_EUNSUPPORTED;
      if (rc)
        return rc;
      if (s.size != 4)
        return MULIB_MOBI_EFORMAT;
      boundary = mulib_azw3_be32_ (s.data);
      if (mulib_mobi_metadata (&r.mobi, 121, 1, &s) != MULIB_MOBI_NOTFOUND)
        return MULIB_MOBI_EFORMAT;
      if (boundary == UINT32_MAX)
        return MULIB_MOBI_EUNSUPPORTED;
      if (boundary <= r.mobi.text_records
          || boundary >= r.mobi.record_count - 1)
        return MULIB_MOBI_EFORMAT;
      rc = mulib_mobi_record (&r.mobi, boundary, &s);
      if (rc)
        return rc;
      if (s.size != 8 || memcmp (s.data, "BOUNDARY", 8))
        return MULIB_MOBI_EFORMAT;
      rc = mulib_mobi_select (&r.mobi, boundary + 1, &r.mobi);
      if (rc)
        return rc;
      if (r.mobi.version != 8)
        return MULIB_MOBI_EFORMAT;
    }
  rc = mulib_mobi_record (&r.mobi, r.mobi.header_record, &header);
  if (rc)
    return rc;
  if (mulib_azw3_be32_ (header.data + 20) < 184)
    return MULIB_MOBI_EFORMAT;
  index = mulib_azw3_be32_ (header.data + 192);
  count = mulib_azw3_be32_ (header.data + 196);
  r.flow_count = 1;
  if (count > 1)
    {
      if (index <= r.mobi.text_records
          || index >= r.mobi.record_count - r.mobi.header_record)
        return MULIB_MOBI_EFORMAT;
      rc = mulib_mobi_record (&r.mobi, r.mobi.header_record + index, &s);
      if (rc)
        return rc;
      if (s.size < 12 || memcmp (s.data, "FDST", 4)
          || mulib_azw3_be32_ (s.data + 4) != 12
          || mulib_azw3_be32_ (s.data + 8) != count
          || count != (s.size - 12) / 8 || (s.size - 12) % 8)
        return MULIB_MOBI_EFORMAT;
      for (i = 0; i < count; ++i)
        {
          size_t start = mulib_azw3_be32_ (s.data + 12 + i * 8),
                 end = mulib_azw3_be32_ (s.data + 16 + i * 8);
          if (start != previous || end < start || end > r.mobi.text_length)
            return MULIB_MOBI_EFORMAT;
          previous = end;
        }
      if (previous != r.mobi.text_length)
        return MULIB_MOBI_EFORMAT;
      r.fdst = s;
      r.flow_count = count;
    }
  *out = r;
  return 0;
}
int
mulib_azw3_text (const mulib_azw3_reader_t *r, size_t limit,
                 mulib_buffer_t *out)
{
  return r ? mulib_mobi_text (&r->mobi, limit, out) : MULIB_MOBI_EINVAL;
}
int
mulib_azw3_flow_range (const mulib_azw3_reader_t *r, size_t index,
                       size_t *offset, size_t *length)
{
  size_t start, end;
  if (!r || !r->mobi.data || !offset || !length || index >= r->flow_count)
    return MULIB_MOBI_EINVAL;
  start = r->fdst.data ? mulib_azw3_be32_ (r->fdst.data + 12 + index * 8) : 0;
  end = r->fdst.data ? mulib_azw3_be32_ (r->fdst.data + 16 + index * 8)
                     : r->mobi.text_length;
  *offset = start;
  *length = end - start;
  return 0;
}
int
mulib_azw3_flow (const mulib_azw3_reader_t *r, size_t index, size_t limit,
                 mulib_buffer_t *out)
{
  mulib_buffer_t text = { 0 };
  size_t offset, length;
  int rc;
  if (!out)
    return MULIB_MOBI_EINVAL;
  rc = mulib_azw3_flow_range (r, index, &offset, &length);
  if (rc)
    return rc;
  rc = mulib_azw3_text (r, limit, &text);
  if (!rc
      && !mulib_buffer_append (out, length ? text.data + offset : NULL,
                               length))
    rc = MULIB_MOBI_ENOMEM;
  mulib_buffer_free (&text);
  return rc;
}
#endif
