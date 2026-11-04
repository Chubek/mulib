#define MULIB_IMPL_TRIE
#define MULIB_IMPL_CORPUS
#include "mulib-corpus.h"
#include <assert.h>
int main(void) {
  mulib_corpus_t corpus = {0};
  assert(mulib_corpus_ingest(&corpus, "Rain rain SUN"));
  assert(corpus.total == 3 && mulib_corpus_count(&corpus, "rain", 4) == 2);
  mulib_corpus_free(&corpus); return 0;
}
