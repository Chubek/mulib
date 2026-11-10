#define MULIB_IMPL_CALLCONV
#include "mulib-callconv.h"
#include <assert.h>
int main(void) {
    mulib_callconv_location_t args[8];size_t stack;
    assert(mulib_callconv_arguments(MULIB_CALLCONV_AMD64_SYSV,8,args,&stack));
    assert(args[0].reg==7&&args[6].reg==-1&&args[7].stack_offset==8&&stack==16);
    return 0;
}
