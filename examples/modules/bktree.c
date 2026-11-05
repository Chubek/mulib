#define MULIB_IMPL_BKTREE
#define MULIB_IMPL_BK_TREES
#define MULIB_IMPL_LEVENSHTEIN
#include "mulib-bktree.h"
#include <assert.h>
static int found(const char *word, size_t distance, void *user) {
  size_t *count = (size_t *)user; assert(!strcmp(word, "hello") && distance == 1); ++*count; return 1;
}
int main(void) {
  mulib_bktree_t tree = {0}; size_t count = 0;
  assert(mulib_bktree_insert(&tree, "hello") && mulib_bktree_insert(&tree, "world"));
  assert(mulib_bktree_search(&tree, "helo", 1, found, &count) == 1 && count == 1);
  mulib_bktree_free(&tree); return 0;
}
