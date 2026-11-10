#define MULIB_IMPL_TYPESYSTEM
#include "mulib-typesystem.h"
#include <assert.h>
int main(void) {
    mulib_typesystem_t types={0};size_t integer,pointer,again;
    assert(mulib_typesystem_intern(&types,(mulib_type_t){MULIB_TYPE_I64,0,0,0},&integer));
    assert(mulib_typesystem_intern(&types,(mulib_type_t){MULIB_TYPE_POINTER,integer,0,0},&pointer));
    assert(mulib_typesystem_intern(&types,(mulib_type_t){MULIB_TYPE_POINTER,integer,9,9},&again)&&again==pointer);
    mulib_typesystem_free(&types);return 0;
}
