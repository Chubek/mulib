/** @file mulib-multipart.h
 * @brief Multipart/form-data boundary and field formatting for simple text
 * fields. C11 single-header implementation. Define MULIB_IMPL_MULTIPART in one
 * translation unit.
 */
#ifndef MULIB_MULTIPART_H
#define MULIB_MULTIPART_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Format one text field, excluding the closing multipart delimiter.
   * Boundary is 1..70 ASCII alphanumeric/hyphen/underscore bytes; name is
   * nonempty without controls, quotes or backslashes. Rejects boundary
   * delimiter lines in value. Return bytes excluding NUL, or 0; output
   * unchanged on error. No overlap. */
  size_t mulib_multipart_field (const char *boundary, const char *name,
                                const char *value, char *out, size_t cap);
  /** Binary-safe form-data parts. Encoder borrows every field. Decoder owns
   * name/filename/content_type, but data borrows the original body. */
  typedef struct mulib_multipart_part {
    char *name, *filename, *content_type;
    const void *data;
    size_t length;
  } mulib_multipart_part_t;
  int mulib_multipart_encode (const char *boundary, const mulib_multipart_part_t *,
                              size_t count, size_t limit, unsigned char **, size_t *);
  /** Initialize cursor=0. Returns 1 part, 0 final delimiter, -1 syntax,
   * -2 limit, -3 allocation. Strict CRLF form-data; no preamble/epilogue.
   * On 1 free decoded metadata before reuse; on error outputs unchanged.
   * limit bounds each part's metadata (zero=32768); body is already bounded by HTTP. */
  int mulib_multipart_next (const char *boundary, const void *, size_t,
                            size_t limit, size_t *cursor, mulib_multipart_part_t *);
  void mulib_multipart_part_free (mulib_multipart_part_t *);

#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_MULTIPART) && !defined(MULIB_MULTIPART_IMPLEMENTED)
#define MULIB_MULTIPART_IMPLEMENTED
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mulib-strbuf.h"
#ifdef __cplusplus
extern "C"
{
#endif

  size_t
  mulib_multipart_field (const char *b, const char *n, const char *v, char *o,
                         size_t cap)
  {
    if (!b || !n || !v || !o || !*b || !*n)
      return 0;
    size_t bn = strlen (b);
    if (bn > 70)
      return 0;
    for (const char *p = b; *p; ++p)
      {
        unsigned char c = (unsigned char)*p;
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
              || (c >= '0' && c <= '9') || c == '-' || c == '_'))
          return 0;
      }
    for (const char *p = n; *p; ++p)
      if ((unsigned char)*p < 32 || (unsigned char)*p == 127 || *p == '"'
          || *p == '\\')
        return 0;
    for (const char *p = v; *p; ++p)
      if ((p == v || p[-1] == '\n') && !strncmp (p, "--", 2)
          && !strncmp (p + 2, b, bn))
        return 0;
    const char *fmt
        = "--%s\r\nContent-Disposition: form-data; name=\"%s\"\r\n\r\n%s\r\n";
    int z = snprintf (NULL, 0, fmt, b, n, v);
    if (z < 0 || (size_t)z >= cap)
      return 0;
    return snprintf (o, cap, fmt, b, n, v) == z ? (size_t)z : 0;
  }

  static int mulib_multipart_safe_ (const char *s, int boundary)
  {
    size_t n;
    if (!s || !*s) return 0;
    n = strlen (s);
    if (boundary && n > 70) return 0;
    for (size_t i = 0; i < n; ++i) {
      unsigned char c = (unsigned char)s[i];
      if (boundary) {
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
            || (c >= '0' && c <= '9') || c == '-' || c == '_')) return 0;
      } else if (c < 32 || c == 127 || c == '"' || c == '\\') return 0;
    }
    return 1;
  }
  static const unsigned char *mulib_multipart_find_ (const unsigned char *p,
      size_t n, const char *needle, size_t k)
  {
    if (k > n) return NULL;
    for (size_t i = 0; i <= n - k; ++i) if (!memcmp (p + i, needle, k)) return p + i;
    return NULL;
  }
  static int mulib_multipart_add_ (mulib_strbuf_t *b, const void *p, size_t n, size_t limit)
  {
    if (n > limit - b->length) return -2;
    return mulib_strbuf_append_n (b, (const char *)p, n) ? 1 : -3;
  }
  int mulib_multipart_encode (const char *boundary, const mulib_multipart_part_t *parts,
      size_t count, size_t limit, unsigned char **out, size_t *length)
  {
    mulib_strbuf_t b = {0};
    char delimiter[75];
    size_t bn;
    int rc = -1;
    if (!out || !length || (!parts && count) || !mulib_multipart_safe_ (boundary, 1)) return -1;
    if (!limit) limit = 16u * 1024u * 1024u;
    bn = strlen (boundary); memcpy (delimiter, "\r\n--", 4); memcpy (delimiter + 4, boundary, bn);
    if (count > limit / (bn + 8)) return -2;
#define MULIB_MULTIPART_ADD_(p,n) do { rc = mulib_multipart_add_ (&b, p, n, limit); if (rc != 1) goto done; } while (0)
#define MULIB_MULTIPART_TEXT_(p) MULIB_MULTIPART_ADD_(p,strlen(p))
    for (size_t i = 0; i < count; ++i) {
      const mulib_multipart_part_t *p = parts + i;
      if (!mulib_multipart_safe_ (p->name, 0) || (p->filename && !mulib_multipart_safe_ (p->filename, 0))
          || (p->content_type && !mulib_multipart_safe_ (p->content_type, 0))
          || (!p->data && p->length)) { rc = -1; goto done; }
      if ((p->length >= bn + 2 && !memcmp (p->data, delimiter + 2, bn + 2))
          || mulib_multipart_find_ ((const unsigned char *)p->data, p->length, delimiter, bn + 4)) { rc = -1; goto done; }
      MULIB_MULTIPART_TEXT_("--"); MULIB_MULTIPART_TEXT_(boundary);
      MULIB_MULTIPART_TEXT_("\r\nContent-Disposition: form-data; name=\"");
      MULIB_MULTIPART_TEXT_(p->name); MULIB_MULTIPART_TEXT_("\"");
      if (p->filename) { MULIB_MULTIPART_TEXT_("; filename=\""); MULIB_MULTIPART_TEXT_(p->filename); MULIB_MULTIPART_TEXT_("\""); }
      if (p->content_type) { MULIB_MULTIPART_TEXT_("\r\nContent-Type: "); MULIB_MULTIPART_TEXT_(p->content_type); }
      MULIB_MULTIPART_TEXT_("\r\n\r\n"); MULIB_MULTIPART_ADD_(p->data,p->length); MULIB_MULTIPART_TEXT_("\r\n");
    }
    MULIB_MULTIPART_TEXT_("--"); MULIB_MULTIPART_TEXT_(boundary); MULIB_MULTIPART_TEXT_("--\r\n");
    *out = (unsigned char *)b.data; *length = b.length; return 1;
done:
    mulib_strbuf_free (&b); return rc;
#undef MULIB_MULTIPART_ADD_
#undef MULIB_MULTIPART_TEXT_
  }
  void mulib_multipart_part_free (mulib_multipart_part_t *p)
  {
    if (p) { free (p->name); free (p->filename); free (p->content_type); memset (p, 0, sizeof *p); }
  }
  static char *mulib_multipart_copy_ (const char *s, size_t n)
  {
    char *p = (char *)malloc (n + 1);
    if (p) { memcpy (p, s, n); p[n] = 0; }
    return p;
  }
  static int mulib_multipart_equal_ (const char *p, size_t n, const char *q)
  {
    if (n != strlen (q)) return 0;
    for (size_t i = 0; i < n; ++i) {
      unsigned char c = (unsigned char)p[i];
      if (c >= 'A' && c <= 'Z') c += 32;
      if (c != (unsigned char)q[i]) return 0;
    }
    return 1;
  }
  static int mulib_multipart_disposition_ (const char *p, size_t n, mulib_multipart_part_t *part)
  {
    size_t i = 0;
    while (i < n && p[i] != ';' && p[i] != ' ') ++i;
    if (!mulib_multipart_equal_ (p, i, "form-data")) return -1;
    while (i < n) {
      size_t key, end, start;
      char **field = NULL;
      while (i < n && (p[i] == ' ' || p[i] == '\t')) ++i;
      if (i == n) break;
      if (p[i++] != ';') return -1;
      while (i < n && (p[i] == ' ' || p[i] == '\t')) ++i;
      key = i;
      while (i < n && p[i] != '=' && p[i] != ' ') ++i;
      end = i;
      while (i < n && p[i] == ' ') ++i;
      if (i == n || p[i++] != '=') return -1;
      while (i < n && p[i] == ' ') ++i;
      if (i == n || p[i++] != '"') return -1;
      start = i;
      while (i < n && p[i] != '"') { if ((unsigned char)p[i] < 32 || p[i] == '\\') return -1; ++i; }
      if (i == n) return -1;
      if (mulib_multipart_equal_ (p + key, end - key, "name")) field = &part->name;
      if (mulib_multipart_equal_ (p + key, end - key, "filename")) field = &part->filename;
      if (field) {
        if (*field) return -1;
        *field = mulib_multipart_copy_ (p + start, i - start);
        if (!*field) return -3;
      }
      ++i;
    }
    return part->name && *part->name ? 1 : -1;
  }
  int mulib_multipart_next (const char *boundary, const void *bytes, size_t n,
      size_t limit, size_t *cursor, mulib_multipart_part_t *out)
  {
    const unsigned char *p = (const unsigned char *)bytes, *end, *body, *next;
    mulib_multipart_part_t part = {0};
    char delimiter[75];
    size_t bn, pos, start;
    int rc = -1, disposition = 0;
    if (!out || !cursor || !p || !mulib_multipart_safe_ (boundary, 1) || *cursor > n) return -1;
    if (!limit) limit = 32768;
    bn = strlen (boundary); pos = *cursor;
    if (pos == n) return n ? 0 : -1;
    memcpy (delimiter, "\r\n--", 4); memcpy (delimiter + 4, boundary, bn);
    if (n - pos < bn + 4 || memcmp (p + pos, delimiter + 2, bn + 2)) return -1;
    pos += bn + 2;
    if (!memcmp (p + pos, "--", 2)) {
      pos += 2;
      if (n - pos == 2 && !memcmp (p + pos, "\r\n", 2)) pos += 2;
      if (pos != n) return -1;
      *cursor = pos; return 0;
    }
    if (memcmp (p + pos, "\r\n", 2)) return -1;
    pos += 2; start = pos;
    for (;;) {
      const unsigned char *colon, *v;
      size_t len;
      end = mulib_multipart_find_ (p + pos, n - pos < limit ? n - pos : limit, "\r\n", 2);
      if (!end) { rc = n - pos >= limit ? -2 : -1; goto fail; }
      len = (size_t)(end - p) - pos;
      if ((size_t)(end - p) + 2 - start > limit) { rc = -2; goto fail; }
      if (!len) { pos += 2; break; }
      for (size_t i = pos; i < (size_t)(end - p); ++i)
        if ((p[i] < 32 && p[i] != '\t') || p[i] == 127) goto fail;
      colon = (const unsigned char *)memchr (p + pos, ':', len);
      if (!colon || colon == p + pos) goto fail;
      for (size_t i = pos; i < (size_t)(colon - p); ++i) {
        unsigned char c = p[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
            || (c >= '0' && c <= '9') || strchr ("!#$%&'*+-.^_`|~", c))) goto fail;
      }
      v = colon + 1; while (v < end && (*v == ' ' || *v == '\t')) ++v;
      if (mulib_multipart_equal_ ((const char *)p + pos, (size_t)(colon - p) - pos, "content-disposition")) {
        if (disposition++) goto fail;
        rc = mulib_multipart_disposition_ ((const char *)v, (size_t)(end - v), &part);
        if (rc != 1) goto fail;
      } else if (mulib_multipart_equal_ ((const char *)p + pos, (size_t)(colon - p) - pos, "content-type")) {
        if (part.content_type) goto fail;
        part.content_type = mulib_multipart_copy_ ((const char *)v, (size_t)(end - v));
        if (!part.content_type) { rc = -3; goto fail; }
      }
      rc = -1; pos = (size_t)(end - p) + 2;
    }
    if (!disposition) goto fail;
    body = p + pos;
    next = body;
    for (;;) {
      next = mulib_multipart_find_ (next, n - (size_t)(next - p), delimiter, bn + 4);
      if (!next) goto fail;
      if (n - (size_t)(next - p) >= bn + 6
          && (!memcmp (next + bn + 4, "--", 2) || !memcmp (next + bn + 4, "\r\n", 2))) break;
      ++next;
    }
    part.data = body; part.length = (size_t)(next - body);
    *cursor = (size_t)(next - p) + 2; *out = part; return 1;
fail:
    mulib_multipart_part_free (&part); return rc;
  }

#ifdef __cplusplus
}
#endif
#endif
