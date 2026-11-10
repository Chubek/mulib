#define MULIB_IMPL_SYMBOLTABLE
#define MULIB_IMPL_COMPILER_SCOPE
#include "mulib-compiler-scope.h"
#include <assert.h>
int main(void) {
    mulib_compiler_scope_t scope={0};uint64_t symbol;
    assert(mulib_compiler_scope_bind(&scope,"answer",42));
    assert(mulib_compiler_scope_resolve(&scope,"answer",&symbol)&&symbol==42);
    mulib_symboltable_free(&scope);return 0;
}
