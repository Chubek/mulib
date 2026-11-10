#define MULIB_IMPL_SYMBOLTABLE
#include "mulib-symboltable.h"
#include <assert.h>
int main(void) {
    mulib_symboltable_t symbols={0};uint64_t value;
    assert(mulib_symboltable_define(&symbols,"x",1)&&mulib_symboltable_enter(&symbols));
    assert(mulib_symboltable_define(&symbols,"x",2));
    assert(mulib_symboltable_lookup(&symbols,"x",&value)&&value==2);
    assert(mulib_symboltable_leave(&symbols)&&mulib_symboltable_lookup(&symbols,"x",&value)&&value==1);
    mulib_symboltable_free(&symbols);return 0;
}
