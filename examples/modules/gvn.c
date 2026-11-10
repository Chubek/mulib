#define MULIB_IMPL_GVN
#include "mulib-gvn.h"
#include <assert.h>
int main(void) {
    mulib_gvn_t table={0};size_t a,b;
    assert(mulib_gvn_intern(&table,1,3,7,1,&a));
    assert(mulib_gvn_intern(&table,1,7,3,1,&b)&&a==b);
    mulib_gvn_free(&table);return 0;
}
