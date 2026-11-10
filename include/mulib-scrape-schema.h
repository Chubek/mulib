/* MuLIB scrape-schema: patched string, integer, real and boolean validation. */
#ifndef MULIB_SCRAPE_SCHEMA_H
#define MULIB_SCRAPE_SCHEMA_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  int mulib_scrape_schema_string (const char *value, int required, size_t max_bytes);
  int mulib_scrape_schema_integer (const char *value, int64_t min_value,
                                   int64_t max_value, int64_t *parsed);
  int mulib_scrape_schema_real (const char *value, double min_value, double max_value,
                                double *parsed);
  int mulib_scrape_schema_boolean (const char *value, int *parsed);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SCRAPE_SCHEMA) && !defined(MULIB_SCRAPE_SCHEMA_IMPLEMENTED)
#define MULIB_SCRAPE_SCHEMA_IMPLEMENTED
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif
  int
  mulib_scrape_schema_string (const char *v, int required, size_t max_bytes)
  {
    if (!v)
      return !required;
    size_t n = strlen (v);
    return n <= max_bytes && (!required || n > 0);
  }
  int
  mulib_scrape_schema_integer (const char *s, int64_t lo, int64_t hi, int64_t *out)
  {
    if (!s || !*s || lo > hi)
      return 0;
    char *end = NULL;
    errno = 0;
    long long n = strtoll (s, &end, 10);
    if (errno == ERANGE || end == s || *end || n < lo || n > hi)
      return 0;
    if (out)
      *out = (int64_t)n;
    return 1;
  }
  int
  mulib_scrape_schema_real (const char *s, double lo, double hi, double *out)
  {
    if (!s || !*s || !isfinite (lo) || !isfinite (hi) || lo > hi)
      return 0;
    char *end = NULL;
    errno = 0;
    double n = strtod (s, &end);
    if (errno == ERANGE || end == s || *end || !isfinite (n) || n < lo || n > hi)
      return 0;
    if (out)
      *out = n;
    return 1;
  }
  int
  mulib_scrape_schema_boolean (const char *s, int *out)
  {
    if (!s)
      return 0;
    int v;
    if (!strcmp (s, "true") || !strcmp (s, "1"))
      v = 1;
    else if (!strcmp (s, "false") || !strcmp (s, "0"))
      v = 0;
    else
      return 0;
    if (out)
      *out = v;
    return 1;
  }
#ifdef __cplusplus
}
#endif
#endif
