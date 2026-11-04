#define MULIB_IMPL_BAYSIAN_RANKING
#include "mulib-baysian_ranking.h"
#include <assert.h>
int main(void) {
  double scores[] = {mulib_bayesian_rank_score(1, 2, 1, 1), mulib_bayesian_rank_score(9, 10, 1, 1)};
  size_t order[2];
  assert(mulib_bayesian_rank(scores, 2, order) && order[0] == 1);
  return 0;
}
