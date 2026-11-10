#define MULIB_IMPL_LEXICAL_TOKEN
#define MULIB_IMPL_LEXER
#define MULIB_IMPL_PARSER_PRATT
#define MULIB_IMPL_SYMBOLTABLE
#define MULIB_IMPL_CONSTEVAL
#include "mulib-consteval.h"
#include <assert.h>
int main(void) {
    mulib_parser_pratt_t tree={0};mulib_symboltable_t scope={0};int64_t value;
    assert(mulib_symboltable_define(&scope,"x",6));
    assert(mulib_parser_pratt_parse("x * (8-1)",9,&tree,NULL));
    assert(mulib_consteval_eval(&tree,&scope,&value)&&value==42);
    mulib_parser_pratt_free(&tree);mulib_symboltable_free(&scope);return 0;
}
