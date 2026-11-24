/** @file mulib-secret-core.h
 * @brief Shared status, byte spans, checked arithmetic, endian and wipe helpers.
 * Define MULIB_IMPL_SECRET_CORE once. Dependencies need their own implementation switches.
 * Development kernel; see manual/secret-kernels.md for security limits.
 */
#ifndef MULIB_SECRET_CORE_H
#define MULIB_SECRET_CORE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef enum {
  MULIB_SC_OK = 0, MULIB_SC_INVALID = -1, MULIB_SC_LIMIT = -2,
  MULIB_SC_AUTH = -3, MULIB_SC_DENIED = -4, MULIB_SC_UNSUPPORTED = -5,
  MULIB_SC_IO = -6, MULIB_SC_NOMEM = -7, MULIB_SC_STATE = -8,
  MULIB_SC_CONFLICT = -9, MULIB_SC_UNCERTAIN = -10, MULIB_SC_NOTFOUND = -11
} mulib_sc_status;
typedef struct { const unsigned char *ptr; size_t len; } mulib_sc_view;
typedef struct { unsigned char *ptr; size_t cap; } mulib_sc_buf;
int mulib_sc_view_ok(mulib_sc_view v);
int mulib_sc_add(size_t a, size_t b, size_t *out);
/* Best-effort wipe of the supplied storage, not of other copies. */
void mulib_sc_wipe(void *p, size_t n);
/* Fixed operation count in source for a public length, not a timing guarantee. */
int mulib_sc_equal(const void *a, const void *b, size_t n);
/* Overlap of valid, nonempty byte ranges on flat-address platforms. */
int mulib_sc_overlap(const void *a, size_t an, const void *b, size_t bn);
void mulib_sc_put64(unsigned char p[8], uint64_t v);
uint64_t mulib_sc_get64(const unsigned char p[8]);
int mulib_sc_name(const char *s, size_t max);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SECRET_CORE) && !defined(MULIB_SECRET_CORE_IMPLEMENTED)
#define MULIB_SECRET_CORE_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_sc_view_ok(mulib_sc_view v)
{
  return v.ptr != NULL || v.len == 0;
}

int mulib_sc_add(size_t a, size_t b, size_t *out)
{
  if (!out || b > SIZE_MAX - a) return MULIB_SC_LIMIT;
  *out = a + b;
  return MULIB_SC_OK;
}

void mulib_sc_wipe(void *p, size_t n)
{
  volatile unsigned char *v = (volatile unsigned char *)p;
  if (v) while (n--) *v++ = 0;
}

int mulib_sc_equal(const void *a, const void *b, size_t n)
{
  const unsigned char *x = (const unsigned char *)a, *y = (const unsigned char *)b;
  unsigned diff = 0;
  if ((!x || !y) && n) return 0;
  for (size_t i = 0; i < n; ++i) diff |= x[i] ^ y[i];
  return diff == 0;
}

int mulib_sc_overlap(const void *a, size_t an, const void *b, size_t bn)
{
  if (!a || !b || !an || !bn) return 0;
  uintptr_t x = (uintptr_t)a, y = (uintptr_t)b;
  /* Compare distances instead of adding lengths to addresses. */
  return x <= y ? y - x < an : x - y < bn;
}

void mulib_sc_put64(unsigned char p[8], uint64_t v)
{
  for (unsigned i = 0; i < 8; ++i) p[7 - i] = (unsigned char)(v >> (8 * i));
}

uint64_t mulib_sc_get64(const unsigned char p[8])
{
  uint64_t v = 0;
  for (unsigned i = 0; i < 8; ++i) v = (v << 8) | p[i];
  return v;
}

int mulib_sc_name(const char *s, size_t max)
{
  if (!s || !*s) return 0;
  for (size_t i = 0; s[i]; ++i) {
    unsigned c = (unsigned char)s[i];
    if (i >= max || !((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
        || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.')) return 0;
  }
  return 1;
}
#ifdef __cplusplus
}
#endif
#endif
