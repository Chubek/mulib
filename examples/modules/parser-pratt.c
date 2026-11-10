#define MULIB_IMPL_LEXICAL_TOKEN
#define MULIB_IMPL_LEXER
#define MULIB_IMPL_PARSER_PRATT
#include "mulib-parser-pratt.h"
#include <assert.h>
int main(void) {
    mulib_parser_pratt_t tree={0};size_t error;
    assert(mulib_parser_pratt_parse("1+2*3",5,&tree,&error)&&error==5);
    assert(tree.nodes[tree.root].opcode==MULIB_PRATT_ADD);
    assert(tree.nodes[tree.nodes[tree.root].right].opcode==MULIB_PRATT_MUL);
    mulib_parser_pratt_free(&tree);return 0;
}
