/* MuLIB: Levenshtein bounded byte edit-distance with O(min(n,m)) memory. C11
 * single-header; define MULIB_IMPL_FUZZYQUERY once. */
#ifndef MULIB_FUZZYQUERY_H
#define MULIB_FUZZYQUERY_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  /* Returns edit distance if <=max_distance, otherwise max_distance+1;
   * SIZE_MAX indicates invalid/overflow/OOM. max_distance must be <SIZE_MAX. */
  size_t mulib_fuzzyquery_distance (const void *a, size_t an, const void *b, size_t bn,
                                    size_t max_distance);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_FUZZYQUERY) && !defined(MULIB_FUZZYQUERY_IMPLEMENTED)
#define MULIB_FUZZYQUERY_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#ifdef __cplusplus
extern "C"
{
#endif
  size_t
  mulib_fuzzyquery_distance (const void *a, size_t an, const void *b, size_t bn,
                             size_t lim)
  {
    if ((!a && an) || (!b && bn) || lim == SIZE_MAX)
      return SIZE_MAX;
    if (an < bn)
      {
        const void *p = a;
        a = b;
        b = p;
        size_t t = an;
        an = bn;
        bn = t;
      }
    if (an - bn > lim)
      return lim + 1;
    if (bn == SIZE_MAX || bn + 1 > SIZE_MAX / sizeof (size_t))
      return SIZE_MAX;
    size_t *dp = (size_t *)malloc ((bn + 1) * sizeof (*dp));
    if (!dp)
      return SIZE_MAX;
    for (size_t j = 0; j <= bn; j++)
      dp[j] = j;
    const unsigned char *x = (const unsigned char *)a, *y = (const unsigned char *)b;
    for (size_t i = 1; i <= an; i++)
      {
        size_t previous = dp[0];
        dp[0] = i;
        size_t rowmin = dp[0];
        for (size_t j = 1; j <= bn; j++)
          {
            size_t diagonal = previous;
            previous = dp[j];
            size_t sub = diagonal + ((x[i - 1] != y[j - 1]) ? 1 : 0), del = dp[j] + 1,
                   ins = dp[j - 1] + 1;
            size_t v = sub < del ? sub : del;
            if (ins < v)
              v = ins;
            dp[j] = v;
            if (v < rowmin)
              rowmin = v;
          }
        if (rowmin > lim)
          {
            free (dp);
            return lim + 1;
          }
      }
    size_t distance = dp[bn];
    free (dp);
    return distance > lim ? lim + 1 : distance;
  }
#ifdef __cplusplus
}
#endif
#endif
