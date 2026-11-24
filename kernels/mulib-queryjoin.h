/* MuLIB: Sorted equi-join pairs over uint64 keys and original row IDs. C11
 * single-header; define MULIB_IMPL_QUERYJOIN once. */
#ifndef MULIB_QUERYJOIN_H
#define MULIB_QUERYJOIN_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct
  {
    uint64_t key, row;
  } mulib_queryjoin_row_t;
  typedef struct
  {
    uint64_t left_row, right_row;
  } mulib_queryjoin_hit_t;
  /* Inputs sorted by key. Emits all duplicate-key pairs; SIZE_MAX on invalid/capacity.
   */
  size_t mulib_queryjoin_merge (const mulib_queryjoin_row_t *a, size_t an,
                                const mulib_queryjoin_row_t *b, size_t bn,
                                mulib_queryjoin_hit_t *out, size_t capacity);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_QUERYJOIN) && !defined(MULIB_QUERYJOIN_IMPLEMENTED)
#define MULIB_QUERYJOIN_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#ifdef __cplusplus
extern "C"
{
#endif
  size_t
  mulib_queryjoin_merge (const mulib_queryjoin_row_t *a, size_t an,
                         const mulib_queryjoin_row_t *b, size_t bn,
                         mulib_queryjoin_hit_t *out, size_t cap)
  {
    if ((!a && an) || (!b && bn) || (!out && cap))
      return SIZE_MAX;
    size_t i = 0, j = 0, total = 0;
    for (size_t k = 1; k < an; k++)
      if (a[k].key < a[k - 1].key)
        return SIZE_MAX;
    for (size_t k = 1; k < bn; k++)
      if (b[k].key < b[k - 1].key)
        return SIZE_MAX;
    while (i < an && j < bn)
      {
        if (a[i].key < b[j].key)
          {
            i++;
            continue;
          }
        if (b[j].key < a[i].key)
          {
            j++;
            continue;
          }
        size_t x = i + 1, y = j + 1;
        while (x < an && a[x].key == a[i].key)
          x++;
        while (y < bn && b[y].key == b[j].key)
          y++;
        if ((x - i) > SIZE_MAX / (y - j) || (x - i) * (y - j) > SIZE_MAX - total)
          return SIZE_MAX;
        total += (x - i) * (y - j);
        i = x;
        j = y;
      }
    if (total > cap)
      return SIZE_MAX;
    i = j = 0;
    size_t at = 0;
    while (i < an && j < bn)
      {
        if (a[i].key < b[j].key)
          {
            i++;
            continue;
          }
        if (b[j].key < a[i].key)
          {
            j++;
            continue;
          }
        size_t x = i + 1, y = j + 1;
        while (x < an && a[x].key == a[i].key)
          x++;
        while (y < bn && b[y].key == b[j].key)
          y++;
        for (size_t p = i; p < x; p++)
          for (size_t q = j; q < y; q++)
            out[at++] = (mulib_queryjoin_hit_t){ a[p].row, b[q].row };
        i = x;
        j = y;
      }
    return total;
  }
#ifdef __cplusplus
}
#endif
#endif
