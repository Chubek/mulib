#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_AST
#define MULIB_IMPL_TREE_PARSING
#define MULIB_IMPL_TERM_REWRITING
#include "mulib-tree_parsing.h"
#include "mulib-term_rewriting.h"
#include <assert.h>
#include <string.h>
int main(void) {
  mulib_ast_t *in = mulib_tree_parse("add(zero,x)", 16, NULL);
  mulib_ast_t *lhs = mulib_tree_parse("add(zero,?x)", 16, NULL), *rhs = mulib_tree_parse("?x", 16, NULL), *out;
  mulib_term_rule_t rule = {lhs, rhs}; size_t steps;
  assert(in && lhs && rhs && mulib_term_rewrite(in, &rule, 1, 8, &out, &steps) == 1);
  assert(steps == 1 && !strcmp(out->production, "x"));
  mulib_ast_destroy(in); mulib_ast_destroy(lhs); mulib_ast_destroy(rhs); mulib_ast_destroy(out); return 0;
}
