/** @file mulib-dom-range.h
 * @brief Validated immutable text/document offsets and range intersections.
 * C11 single-header module. Define MULIB_IMPL_DOM_RANGE in exactly one C
 * translation unit.
 *
 */
#ifndef MULIB_DOM_RANGE_H
#define MULIB_DOM_RANGE_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct mulib_dom_range
  {
    size_t start, end;
    uint64_t document_version;
  } mulib_dom_range_t;
  /** @brief See manual/mulib-dom-range.md for this operation's contract. */
  int mulib_dom_range_make (size_t start, size_t end, size_t document_length,
                            uint64_t version, mulib_dom_range_t *out);
  /** @brief See manual/mulib-dom-range.md for this operation's contract. */
  int mulib_dom_range_intersect (mulib_dom_range_t a, mulib_dom_range_t b,
                                 mulib_dom_range_t *out);
  /** @brief See manual/mulib-dom-range.md for this operation's contract. */
  int mulib_dom_range_shift (mulib_dom_range_t r, int64_t delta, size_t length,
                             mulib_dom_range_t *out);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_DOM_RANGE_H */
#ifdef MULIB_IMPL_DOM_RANGE
#ifndef MULIB_IMPL_DOM_RANGE_ONCE
#define MULIB_IMPL_DOM_RANGE_ONCE
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
mulib_dom_range_make (size_t a, size_t b, size_t len, uint64_t ver,
                      mulib_dom_range_t *o)
{
  if (!o || a > b || b > len)
    return 0;
  *o = (mulib_dom_range_t){ a, b, ver };
  return 1;
}
int
mulib_dom_range_intersect (mulib_dom_range_t a, mulib_dom_range_t b,
                           mulib_dom_range_t *o)
{
  if (!o || a.start > a.end || b.start > b.end
      || a.document_version != b.document_version)
    return 0;
  size_t s = a.start > b.start ? a.start : b.start,
         e = a.end < b.end ? a.end : b.end;
  if (s > e)
    return 0;
  *o = (mulib_dom_range_t){ s, e, a.document_version };
  return 1;
}
int
mulib_dom_range_shift (mulib_dom_range_t r, int64_t d, size_t len,
                       mulib_dom_range_t *o)
{
  if (!o || r.start > r.end || r.end > len)
    return 0;
  size_t start, end;
  if (d >= 0)
    {
      if ((uint64_t)d > SIZE_MAX - r.end)
        return 0;
      start = r.start + (size_t)d;
      end = r.end + (size_t)d;
    }
  else
    {
      uint64_t x = (uint64_t)(-(d + 1)) + 1;
      if (x > r.start)
        return 0;
      start = r.start - (size_t)x;
      end = r.end - (size_t)x;
    }
  if (end > len)
    return 0;
  *o = (mulib_dom_range_t){ start, end, r.document_version };
  return 1;
}
#endif
#endif
