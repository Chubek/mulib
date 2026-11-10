#define MULIB_IMPL_LEXICAL_TOKEN
#define MULIB_IMPL_LEXER
#define MULIB_IMPL_PARSER_PRATT
#define MULIB_IMPL_SYMBOLTABLE
#define MULIB_IMPL_TYPESYSTEM
#define MULIB_IMPL_TYPECHECK
#include "mulib-typecheck.h"
#include <assert.h>
int main(void) {
    mulib_parser_pratt_t tree={0};mulib_typesystem_t types={0};size_t integer,boolean,result;
    assert(mulib_typesystem_intern(&types,(mulib_type_t){MULIB_TYPE_I64,0,0,0},&integer));
    assert(mulib_typesystem_intern(&types,(mulib_type_t){MULIB_TYPE_BOOL,0,0,0},&boolean));
    assert(mulib_parser_pratt_parse("6 < 7",5,&tree,NULL));
    assert(mulib_typecheck_expr(&tree,&types,NULL,&result)&&result==boolean);
    mulib_parser_pratt_free(&tree);mulib_typesystem_free(&types);return 0;
}
