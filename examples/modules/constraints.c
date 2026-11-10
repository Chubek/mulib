#define MULIB_IMPL_CONSTRAINTS
#include "mulib-constraints.h"
#include <assert.h>
int main(void) {
    mulib_constraints_t constraints={0};size_t a,b,type;
    assert(mulib_constraints_new(&constraints,&a)&&mulib_constraints_new(&constraints,&b));
    assert(mulib_constraints_unify(&constraints,a,b)&&mulib_constraints_bind(&constraints,a,3));
    assert(mulib_constraints_type(&constraints,b,&type)&&type==3);
    mulib_constraints_free(&constraints);return 0;
}
