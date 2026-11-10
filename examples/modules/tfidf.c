#define MULIB_IMPL_TFIDF
#include "mulib-tfidf.h"
#include <assert.h>
int
main (void)
{
  double common = mulib_tfidf_score (3, 100, 90, 1),
         rare = mulib_tfidf_score (3, 100, 2, 1);
  assert (isfinite (rare) && rare > common);
  return 0;
}
