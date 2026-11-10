#define MULIB_IMPL_LEXER
#define MULIB_IMPL_LEXICAL_TOKEN
#include "mulib-lexer.h"
#include <assert.h>
int main(void) {
    mulib_lexer_t lexer;mulib_lexical_token_t token;
    mulib_lexer_init(&lexer,"x + 42",6);
    assert(mulib_lexer_next(&lexer,&token)==1&&mulib_lexical_token_is(&token,"x"));
    assert(mulib_lexer_next(&lexer,&token)==1&&token.kind==MULIB_TOKEN_OPERATOR);
    assert(mulib_lexer_next(&lexer,&token)==1&&token.kind==MULIB_TOKEN_INTEGER&&token.offset==4);
    assert(!mulib_lexer_next(&lexer,&token));return 0;
}
