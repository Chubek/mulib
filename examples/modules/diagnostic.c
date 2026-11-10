#define MULIB_IMPL_DIAGNOSTIC
#include "mulib-diagnostic.h"
#include <assert.h>
int main(void) {
    mulib_diagnostic_list_t diagnostics={0};
    assert(mulib_diagnostic_add(&diagnostics,100,MULIB_DIAG_ERROR,0,4,3,"unknown name"));
    assert(diagnostics.errors==1&&!strcmp(diagnostics.items[0].message,"unknown name"));
    mulib_diagnostic_free(&diagnostics);return 0;
}
