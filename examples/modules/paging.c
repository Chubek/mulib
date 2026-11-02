#define MULIB_IMPL_PAGING
#include "mulib-paging.h"
#include <assert.h>
int main(void) {
    uint64_t pte;
    assert(!mulib_paging_entry(0x2000, 48, MULIB_PAGING_PRESENT | MULIB_PAGING_WRITE, &pte));
    assert(pte == 0x2003 && mulib_paging_canonical(UINT64_C(0xffff800000000000), 48));
    return 0;
}
