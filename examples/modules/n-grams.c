#define MULIB_IMPL_N_GRAMS
#include "mulib-n-grams.h"
#include <assert.h>
static int visit(const unsigned char *s, size_t n, size_t offset, void *ctx) {
  (void)s; (void)n; (void)offset; ++*(size_t *)ctx; return 1;
}
int main(void) {
  size_t count = 0;
  assert(mulib_ngrams("hello", 5, 2, visit, &count) == 4 && count == 4);
  return 0;
}
