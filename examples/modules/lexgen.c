#define MULIB_IMPL_LEXGEN
#include "mulib-lexgen.h"
#include <assert.h>
int main(void) {
    mulib_lexgen_rule_t rules[]={{MULIB_LEXGEN_LITERAL,"if",1},{MULIB_LEXGEN_IDENT,NULL,2}};
    size_t matched;int token;
    assert(mulib_lexgen_match(rules,2,"if",2,&matched,&token)==1&&token==1);
    assert(mulib_lexgen_match(rules,2,"iffy",4,&matched,&token)==1&&token==2&&matched==4);
    return 0;
}
