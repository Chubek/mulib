#define MULIB_IMPL_MODULE_RESOLVER
#include "mulib-module-resolver.h"
#include <assert.h>
int main(void) {
    mulib_module_resolver_t resolver={0};
    assert(mulib_module_resolver_add(&resolver,"math","src/math.mu"));
    assert(!strcmp(mulib_module_resolver_get(&resolver,"math"),"src/math.mu"));
    mulib_module_resolver_free(&resolver);return 0;
}
