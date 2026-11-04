#define MULIB_IMPL_TRIE
#include "mulib-trie.h"
#include <assert.h>
int main(void) {
  mulib_trie_t trie = {0}; uint64_t value;
  assert(mulib_trie_set(&trie, "cat", 3, 7));
  assert(mulib_trie_get(&trie, "cat", 3, &value) && value == 7);
  assert(mulib_trie_remove(&trie, "cat", 3));
  mulib_trie_free(&trie); return 0;
}
