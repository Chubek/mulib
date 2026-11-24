/** @file mulib-der.h
 * @brief Bounded canonical ASN.1 DER TLVs and unsigned INTEGER decoding.
 * Define MULIB_IMPL_DER once. Values borrow input; no allocation. */
#ifndef MULIB_DER_H
#define MULIB_DER_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_der_cursor { const unsigned char *data; size_t length, offset; } mulib_der_cursor_t;
typedef struct mulib_der_value {
  unsigned tag;
  const unsigned char *encoded, *data;
  size_t encoded_length, length;
} mulib_der_value_t;
/** Read one TLV: 1 success, 0 exact end, -1 invalid/truncated/noncanonical.
 * High-tag-number encodings and indefinite lengths are unsupported/rejected.
 * Failure/end leaves cursor and output unchanged. */
int mulib_der_next (mulib_der_cursor_t *, mulib_der_value_t *);
/** Read expected tag, unchanged on mismatch/error. */
int mulib_der_take (mulib_der_cursor_t *, unsigned tag, mulib_der_value_t *);
mulib_der_cursor_t mulib_der_contents (const mulib_der_value_t *);
/** Borrow the minimal unsigned magnitude of a nonnegative canonical INTEGER.
 * Zero is encoded by one zero byte. Output unchanged on invalid input. */
int mulib_der_unsigned (const mulib_der_value_t *, const unsigned char **, size_t *);
int mulib_der_u32 (const mulib_der_value_t *, uint32_t *);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_DER) && !defined(MULIB_DER_IMPLEMENTED)
#define MULIB_DER_IMPLEMENTED
int mulib_der_next (mulib_der_cursor_t *c, mulib_der_value_t *out)
{
  mulib_der_value_t v;
  size_t pos, start, n;
  unsigned first;
  if (!c || !out || c->offset > c->length || (!c->data && c->length)) return -1;
  if (c->offset == c->length) return 0;
  start = pos = c->offset;
  if (c->length - pos < 2) return -1;
  v.tag = c->data[pos++];
  if (!(v.tag & 0xdf) || (v.tag & 31) == 31) return -1;
  first = c->data[pos++]; n = first;
  if (first & 128) {
    unsigned count = first & 127;
    if (!count || count > sizeof (size_t) || count > c->length - pos || !c->data[pos]) return -1;
    n = 0;
    for (unsigned i = 0; i < count; ++i) n = (n << 8) | c->data[pos++];
    if (n < 128) return -1;
  }
  if (n > c->length - pos) return -1;
  v.encoded = c->data + start; v.data = c->data + pos;
  v.length = n; v.encoded_length = pos - start + n;
  c->offset = pos + n; *out = v; return 1;
}
int mulib_der_take (mulib_der_cursor_t *c, unsigned tag, mulib_der_value_t *out)
{
  mulib_der_cursor_t copy; mulib_der_value_t v;
  if (!c || !out) return 0;
  copy = *c;
  if (mulib_der_next (&copy, &v) != 1 || v.tag != tag) return 0;
  *c = copy; *out = v; return 1;
}
mulib_der_cursor_t mulib_der_contents (const mulib_der_value_t *v)
{
  mulib_der_cursor_t c = {NULL, 0, 0};
  if (v) { c.data = v->data; c.length = v->length; }
  return c;
}
int mulib_der_unsigned (const mulib_der_value_t *v, const unsigned char **out, size_t *length)
{
  size_t skip;
  if (!v || v->tag != 2 || !v->data || !v->length || (v->data[0] & 128) || !out || !length) return 0;
  skip = v->length > 1 && !v->data[0];
  if (skip && !(v->data[1] & 128)) return 0;
  *out = v->data + skip; *length = v->length - skip; return 1;
}
int mulib_der_u32 (const mulib_der_value_t *v, uint32_t *out)
{
  const unsigned char *p; size_t n; uint32_t value = 0;
  if (!out || !mulib_der_unsigned (v, &p, &n) || n > 4) return 0;
  for (size_t i = 0; i < n; ++i) value = (value << 8) | p[i];
  *out = value; return 1;
}
#endif
