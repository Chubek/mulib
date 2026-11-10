#define MULIB_IMPL_SYNTAX_TREE
#include "mulib-syntax-tree.h"
#include <assert.h>
int main(void) {
    mulib_syntax_tree_t tree={0};size_t root,child;
    assert(mulib_syntax_tree_add(&tree,SIZE_MAX,0,0,10,&root));
    assert(mulib_syntax_tree_add(&tree,root,1,2,3,&child));
    assert(mulib_syntax_tree_validate(&tree)&&tree.nodes[root].first_child==child);
    mulib_syntax_tree_free(&tree);return 0;
}
