#define MULIB_IMPL_LEVENSHTEIN
#define MULIB_IMPL_BK_TREES
#include "mulib-bk-trees.h"
#include <assert.h>
static int visit(const char *word, size_t distance, void *context) {
  (void)word; (void)distance; ++*(size_t *)context; return 1;
}
int main(void) {
  mulib_bk_tree_t tree = {0}; size_t count = 0;
  assert(mulib_bk_tree_insert(&tree, "book") && mulib_bk_tree_insert(&tree, "books"));
  assert(mulib_bk_tree_search(&tree, "book", 1, visit, &count) == 2 && count == 2);
  mulib_bk_tree_free(&tree); return 0;
}
