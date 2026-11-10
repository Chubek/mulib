#define MULIB_IMPL_BM25
#include "mulib-bm25.h"
#include <assert.h>
#include <stdio.h>
int
main (void)
{
  double tf[] = { 3, 1 }, len[] = { 10, 100 }, avg[] = { 20, 100 }, weight[] = { 2, 1 },
         b[] = { 0.75, 0.75 };
  double score = mulib_bm25f_score (tf, len, avg, weight, b, 2, 10, 1000, 1.2);
  assert (isfinite (score) && score > 0);
  printf ("BM25F: %.6f\n", score);
  return 0;
}
