#define MULIB_IMPL_SYMBOL
#define MULIB_IMPL_MATHML
#define MULIB_IMPL_XML
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#include "mulib-symbol.h"
#include <assert.h>
#include <stdio.h>
int main(void){
    mulib_symbol_registry_t *r=mulib_symbol_create();
    mulib_symbol_t symbol={"novel","⋆","\\star","mo",MULIB_SYMBOL_INFIX,35};
    const mulib_symbol_t *alpha=mulib_symbol_find(r,"alpha");
    assert(r&&alpha&&mulib_symbol_define(r,&symbol));
    printf("%s -> %s\n",alpha->utf8,alpha->latex);
    mulib_symbol_destroy(r);return 0;
}
