#define MULIB_IMPL_TRIE
#define MULIB_IMPL_TRIE_TRAVERSAL
#include "mulib-trie_traversal.h"
#include <assert.h>
int main(void) {
  mulib_trie_t trie = {0}; size_t length; uint64_t value;
  assert(mulib_trie_set(&trie, "cat", 3, 7));
  assert(mulib_trie_longest_prefix(&trie, "cats", 4, &length, &value) && length == 3 && value == 7);
  mulib_trie_free(&trie); return 0;
}
