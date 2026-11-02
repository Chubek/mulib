#define MULIB_IMPL_IO
#include "mulib-io.h"
#include <assert.h>
int main(void) {
    /* Default build must not execute hardware instructions in user mode. */
    assert(mulib_io_out8(0x80, 0) == -1);
    return 0;
}
