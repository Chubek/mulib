#define MULIB_IMPL_LEXICAL_TOKEN
#define MULIB_IMPL_LEXER
#define MULIB_IMPL_PARSER_PRATT
#define MULIB_IMPL_SYMBOLTABLE
#define MULIB_IMPL_NAME_RESOLUTION
#include "mulib-name-resolution.h"
#include <assert.h>
int main(void) {
    mulib_parser_pratt_t tree={0};mulib_symboltable_t symbols={0};size_t ids[3],bad;
    assert(mulib_parser_pratt_parse("x+1",3,&tree,NULL)&&mulib_symboltable_define(&symbols,"x",42));
    assert(mulib_name_resolution_run(&tree,&symbols,ids,3,&bad)&&ids[0]==42&&ids[1]==SIZE_MAX);
    mulib_parser_pratt_free(&tree);mulib_symboltable_free(&symbols);return 0;
}
