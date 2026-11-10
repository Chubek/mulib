#define MULIB_IMPL_STACKMAP
#include "mulib-stackmap.h"
#include <assert.h>
int main(void) {
    mulib_stackmap_t map={0};mulib_stackmap_entry_t point;
    assert(mulib_stackmap_add(&map,16,3,4));
    assert(mulib_stackmap_find(&map,16,&point)&&point.pointer_registers==3&&point.pointer_spills==4);
    mulib_stackmap_free(&map);return 0;
}
