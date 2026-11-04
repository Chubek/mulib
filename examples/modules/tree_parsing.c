#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_AST
#define MULIB_IMPL_TREE_PARSING
#include "mulib-tree_parsing.h"
#include <assert.h>
#include <string.h>
int main(void) {
  size_t offset; mulib_ast_t *tree = mulib_tree_parse("add(x,mul(y,z))", 32, &offset);
  assert(tree && !strcmp(tree->production, "add") && offset == 15);
  mulib_ast_destroy(tree); return 0;
}
