#define MULIB_IMPL_LEXICAL_TOKEN
#include "mulib-lexical-token.h"
#include <assert.h>
int main(void) {
    mulib_lexical_token_t token={MULIB_TOKEN_IDENTIFIER,"answer + 1",6,0};
    assert(mulib_lexical_token_is(&token,"answer"));
    assert(!mulib_lexical_token_is(&token,"answer + 1"));return 0;
}
