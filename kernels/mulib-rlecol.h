/* MuLIB: Run-length encoding of unsigned 64-bit column values. C11 single-header;
 * define MULIB_IMPL_RLECOL once. */
#ifndef MULIB_RLECOL_H
#define MULIB_RLECOL_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct
  {
    uint64_t value, count;
  } mulib_rlecol_run_t;
  /* Returns run count; SIZE_MAX invalid/capacity insufficiency, no partial writes on
   * capacity error. */
  size_t mulib_rlecol_encode (const uint64_t *values, size_t n,
                              mulib_rlecol_run_t *runs, size_t capacity);
  size_t mulib_rlecol_decode (const mulib_rlecol_run_t *runs, size_t n, uint64_t *out,
                              size_t capacity);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_RLECOL) && !defined(MULIB_RLECOL_IMPLEMENTED)
#define MULIB_RLECOL_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#ifdef __cplusplus
extern "C"
{
#endif
  size_t
  mulib_rlecol_encode (const uint64_t *v, size_t n, mulib_rlecol_run_t *out, size_t cap)
  {
    if ((!v && n) || (!out && cap))
      return SIZE_MAX;
    size_t count = 0;
    for (size_t i = 0; i < n;)
      {
        size_t j = i + 1;
        while (j < n && v[j] == v[i] && j - i < UINT64_MAX)
          j++;
        count++;
        i = j;
      }
    if (count > cap)
      return SIZE_MAX;
    for (size_t i = 0, k = 0; i < n; k++)
      {
        size_t j = i + 1;
        while (j < n && v[j] == v[i] && j - i < UINT64_MAX)
          j++;
        out[k] = (mulib_rlecol_run_t){ v[i], (uint64_t)(j - i) };
        i = j;
      }
    return count;
  }
  size_t
  mulib_rlecol_decode (const mulib_rlecol_run_t *runs, size_t n, uint64_t *out,
                       size_t cap)
  {
    if ((!runs && n) || (!out && cap))
      return SIZE_MAX;
    size_t total = 0;
    for (size_t i = 0; i < n; i++)
      {
        if (!runs[i].count || runs[i].count > cap - total)
          return SIZE_MAX;
        total += (size_t)runs[i].count;
      }
    size_t pos = 0;
    for (size_t i = 0; i < n; i++)
      for (uint64_t j = 0; j < runs[i].count; j++)
        out[pos++] = runs[i].value;
    return pos;
  }
#ifdef __cplusplus
}
#endif
#endif
